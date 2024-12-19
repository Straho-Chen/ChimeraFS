/*
 * agent.h
 *
 *  Created on: Sep 2, 2021
 *      Author: dzhou
 */

#ifndef __AGENT_H_
#define __AGENT_H_

#include "odinfs_config.h"
#include <linux/pgtable.h>

struct odinfs_agent_tasks {
	unsigned long kuaddr;
	unsigned long size;
};

int odinfs_init_agents(int cpus, int sockets);
void odinfs_agents_fini(void);

extern int odinfs_dele_thrds;
extern int cpus_per_socket;

#endif /* __AGENT_H_ */
