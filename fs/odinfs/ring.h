/*
 * ring.h
 *
 *  Created on: Sep 6, 2021
 *      Author: dzhou
 */

#ifndef __RING_H_
#define __RING_H_

#include "delegation.h"
#include "odinfs_config.h"

extern int odinfs_num_of_rings_per_socket;

#if ODINFS_SOLROS_RING_BUFFER
#include <solros_ring_buffer_api.h>
typedef struct solros_ring_buffer_t odinfs_ring_buffer_t;
#else
#include "simple_ring.h"
typedef struct sodinfs_ring_buffer odinfs_ring_buffer_t;
#endif

extern odinfs_ring_buffer_t
	*odinfs_ring_buffer[ODINFS_MAX_SOCKET][ODINFS_MAX_AGENT_PER_SOCKET];

int odinfs_init_ring_buffers(int sockets);

static inline int odinfs_send_request(odinfs_ring_buffer_t *ring,
				      struct odinfs_delegation_request *request)
{
#if ODINFS_SOLROS_RING_BUFFER
	return solros_ring_enqueue(ring, request,
				   sizeof(struct odinfs_delegation_request), 1);
#else
	return odinfs_sr_send_request(ring, request);
#endif
}

static inline int odinfs_recv_request(odinfs_ring_buffer_t *ring,
				      struct odinfs_delegation_request *request)
{
#if ODINFS_SOLROS_RING_BUFFER
	return solros_ring_dequeue(ring, request, NULL, 1);
#else
	return odinfs_sr_receive_request(ring, request);
#endif
}

void odinfs_fini_ring_buffers(void);

#endif /* __RING_H_ */
