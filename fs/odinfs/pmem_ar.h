/*
 * pmem_ar.h
 *
 *  Created on: Jul 13, 2021
 *      Author: dzhou
 */

#ifndef SODINFS_PMEM_AR_H_
#define SODINFS_PMEM_AR_H_

#define SODINFS_MAJOR 263
#define SODINFS_DEV_NAME "odinfs_pmem_ar"
#define SODINFS_DEV_INSTANCE_NAME "odinfs_pmem_ar0"
#define SODINFS_PMEM_AR_NUM 1

#define ODINFS_PMEM_AR_MAX_DEVICE 8

struct pmem_arg_info {
	/* number of devices to add*/
	int num;

	/* path to these devices */
	char *paths[ODINFS_PMEM_AR_MAX_DEVICE];
};

#define SODINFS_PMEM_AR_CMD_CREATE 0
#define SODINFS_PMEM_AR_CMD_DELETE 1

#endif /* SODINFS_PMEM_AR_H_ */
