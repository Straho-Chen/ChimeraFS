#ifndef __SIMPLE_RING_H_
#define __SIMPLE_RING_H_

#include <linux/spinlock.h>

#include "delegation.h"
#include "odinfs_config.h"

struct sodinfs_ring_buffer_entry {
	/* This is not perfect, but keep it for the sake of simplicity */
	struct odinfs_delegation_request request;
	volatile int valid;
};

struct sodinfs_ring_buffer {
	struct sodinfs_ring_buffer_entry *requests;
	spinlock_t spinlock;
	int producer_idx, comsumer_idx, num_of_entry, entry_size;
};

struct sodinfs_ring_buffer *odinfs_sr_create(int socket, int entry_size,
					     int size);

void odinfs_sr_destroy(struct sodinfs_ring_buffer *ring);

int odinfs_sr_send_request(struct sodinfs_ring_buffer *ring, void *from);

int odinfs_sr_receive_request(struct sodinfs_ring_buffer *ring, void *to);

#endif
