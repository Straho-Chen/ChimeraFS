/*
 * pmem_ar.h
 *
 *  Created on: Jul 13, 2021
 *      Author: dzhou
 */

#ifndef _PMEM_AR_H
#define _PMEM_AR_H

#define SLFS_MAJOR 263
#define SLFS_DEV_NAME "pmem_ar"
#define SLFS_DEV_INSTANCE_NAME "pmem_ar0"
#define SLFS_PMEM_AR_NUM 1

// If we have dual CPU, then we have two sockets, and two pmem device will be passed to this create driver.
#define SLFS_PMEM_AR_MAX_DEVICE 8

struct pmem_arg_info {
	/* number of devices to add*/
	int num;

	/* path to these devices */
	char *paths[SLFS_PMEM_AR_MAX_DEVICE];
};

#define SLFS_PMEM_AR_CMD_CREATE 0
#define SLFS_PMEM_AR_CMD_DELETE 1

#endif