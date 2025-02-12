#ifndef __AGENT_H_
#define __AGENT_H_

#include <linux/pgtable.h>
#include "nova_config.h"

struct nova_agent_tasks {
	unsigned long kuaddr;
	unsigned long size;
};

int nova_init_agents(int cpus, int sockets);
void nova_agents_fini(void);

extern int nova_dele_thrds;
extern int cpus_per_socket;

#endif /* __AGENT_H_ */
