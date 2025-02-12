#ifndef __RING_H_
#define __RING_H_

#include "agent.h"
#include "delegation.h"

#include <solros_ring_buffer_api.h>
typedef struct solros_ring_buffer_t nova_ring_buffer_t;

extern int nova_num_of_rings_per_socket;

extern nova_ring_buffer_t
	*nova_ring_buffer[NOVA_MAX_SOCKET][NOVA_MAX_AGENT_PER_SOCKET];

int nova_init_ring_buffers(int sockets);

void nova_fini_ring_buffers(void);

int nova_send_request(nova_ring_buffer_t *ring,
		      struct nova_delegation_request *request);

int nova_recv_request(nova_ring_buffer_t *ring,
		      struct nova_delegation_request *request);

#endif /* __RING_H_ */
