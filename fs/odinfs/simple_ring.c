#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include "delegation.h"
#include "odinfs_config.h"
#include "odinfs_stats.h"
#include "ring.h"
#include "simple_ring.h"

#include "odinfs.h"

struct sodinfs_ring_buffer *odinfs_sr_create(int socket, int entry_size,
					     int size)
{
	struct sodinfs_ring_buffer *ret;

	ret = kmalloc_node(sizeof(struct sodinfs_ring_buffer), GFP_KERNEL,
			   socket);

	if (ret == NULL)
		return NULL;

	/* kcalloc cannot allocate more than 4MB size in our kernel */
	ret->requests = vzalloc_node(size, socket);

	if (ret->requests == NULL) {
		kfree(ret);
		return NULL;
	}

	ret->comsumer_idx = 0;
	ret->producer_idx = 0;
	ret->entry_size = entry_size;
	/* See comments in simple_ring.h */
	ret->num_of_entry = size / (sizeof(struct sodinfs_ring_buffer_entry));

	spin_lock_init(&ret->spinlock);

	return ret;
}

void odinfs_sr_destroy(struct sodinfs_ring_buffer *ring)
{
	if (ring->requests)
		vfree(ring->requests);

	kfree(ring);
}

int odinfs_sr_send_request(struct sodinfs_ring_buffer *ring, void *from)
{
	unsigned long irq;
	int ret = 0;

	spin_lock_irqsave(&ring->spinlock, irq);

	if (ring->requests[ring->producer_idx].valid) {
		ret = -EAGAIN;
		goto out;
	}

	memcpy(&(ring->requests[ring->producer_idx].request), from,
	       ring->entry_size);

	ring->requests[ring->producer_idx].valid = 1;

	ring->producer_idx = (ring->producer_idx + 1) % (ring->num_of_entry);

out:
	spin_unlock_irqrestore(&ring->spinlock, irq);
	return ret;
}

int odinfs_sr_receive_request(struct sodinfs_ring_buffer *ring, void *to)
{
	unsigned long irq;
	int ret = 0;

	/* Spin to wait for a new entry */
	spin_lock_irqsave(&ring->spinlock, irq);

	if (!ring->requests[ring->comsumer_idx].valid) {
		ret = -EAGAIN;
		goto out;
	}

	memcpy(to, &(ring->requests[ring->comsumer_idx].request),
	       ring->entry_size);

	ring->requests[ring->comsumer_idx].valid = 0;

	ring->comsumer_idx = (ring->comsumer_idx + 1) % ring->num_of_entry;

out:
	spin_unlock_irqrestore(&ring->spinlock, irq);
	return ret;
}
