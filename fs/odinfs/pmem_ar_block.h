/*
 * pmem_ar_block.h
 *
 *  Created on: Jul 14, 2021
 *      Author: dzhou
 */

#ifndef SODINFS_PMEM_AR_BLOCK_H_
#define SODINFS_PMEM_AR_BLOCK_H_

#include "pmem_ar.h"

extern struct pmem_ar_dev pmem_ar_dev;

struct pmem_ar_dev {
	/* Linux kernel block device fields */
	struct gendisk *gd;

	/* number of pmem devices in the pmem array*/
	int elem_num;

	/* bdevs of pmem devices in the pmem array */
	struct block_device *bdevs[ODINFS_PMEM_AR_MAX_DEVICE];

	struct dax_device *dax_dev[ODINFS_PMEM_AR_MAX_DEVICE];

	unsigned long start_virt_addr[ODINFS_PMEM_AR_MAX_DEVICE];
	unsigned long size_in_bytes[ODINFS_PMEM_AR_MAX_DEVICE];
};

int sodinfs_pmem_ar_init_block(void);
void sodinfs_pmem_ar_exit_block(void);

void sodinfs_put_pmem_ar(void);

#endif /* SODINFS_PMEM_AR_BLOCK_H_ */
