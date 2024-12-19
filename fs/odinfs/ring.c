#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "agent.h"
#include "delegation.h"
#include "odinfs_config.h"
#include "odinfs_stats.h"
#include "ring.h"

#include "odinfs.h"

#if ODINFS_SOLROS_RING_BUFFER
#include <solros_ring_buffer_api.h>
#else
#include "simple_ring.h"
#endif

odinfs_ring_buffer_t
	*odinfs_ring_buffer[ODINFS_MAX_SOCKET][ODINFS_MAX_AGENT_PER_SOCKET];

static int odinfs_number_of_sockets;
int odinfs_num_of_rings_per_socket;

#if ODINFS_SOLROS_RING_BUFFER
/* 64B alignment */
#define ODINFS_RING_ALIGN (64)
#endif

int odinfs_init_ring_buffers(int sockets)
{
	int i, j;

	odinfs_number_of_sockets = sockets;
	odinfs_num_of_rings_per_socket = odinfs_dele_thrds;

	for (i = 0; i < sockets; i++)
		for (j = 0; j < odinfs_dele_thrds; j++) {
			odinfs_ring_buffer_t *ret;
			/* nonblocking ring buffer for each socket */
#if ODINFS_SOLROS_RING_BUFFER
			ret = solros_ring_create(ODINFS_RING_SIZE,
						 ODINFS_RING_ALIGN, i, 0);
#else
			ret = odinfs_sr_create(
				i, sizeof(struct odinfs_delegation_request),
				ODINFS_RING_SIZE);
#endif

			if (ret == NULL)
				goto err;

			odinfs_ring_buffer[i][j] = ret;
		}

	return 0;

err:
	odinfs_fini_ring_buffers();

	return -ENOMEM;
}

void odinfs_fini_ring_buffers(void)
{
	int i, j;

	for (i = 0; i < odinfs_number_of_sockets; i++)
		for (j = 0; j < odinfs_num_of_rings_per_socket; j++) {
			if (!odinfs_ring_buffer[i][j])
				continue;

#if ODINFS_SOLROS_RING_BUFFER
			solros_ring_destroy(odinfs_ring_buffer[i][j]);
#else
			odinfs_sr_destroy(odinfs_ring_buffer[i][j]);
#endif
		}
}
