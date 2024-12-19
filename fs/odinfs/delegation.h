/*
 * delegation.h
 *
 *  Created on: Sep 3, 2021
 *      Author: dzhou
 */

#ifndef __DELEGATION_H_
#define __DELEGATION_H_

#include <linux/types.h>

#include "odinfs.h"

#define ODINFS_REQUEST_READ 0
#define ODINFS_REQUEST_WRITE 1

/*
 * TODO: This needs to be further optimized, we want to minimize the
 * size of copying
 */

/* TODO: verify the correctness of cacheline break */
struct odinfs_delegation_request {
	/* read, write */
	int type;

	/*
   * for read requests, memset [uaddr, uaddr + size) with 0.
   * for write request, memset [kaddr, kaddr + size) with 0.
   * Useful for sparse file and delegating memset.
   */

	int zero;

	/* for write request, flush the cache or not. */
	int flush_cache;

	/* for write request, do sfence or not. */
	int sfence;

	/* user address, kernel address, size of the request */
	unsigned long uaddr, kaddr, bytes;

	struct mm_struct *mm;

	atomic_t *notify_cnt;

	int wait_hint;

	char pad[8];
};

/* TODO: verify the correctness of cacheline break */
struct odinfs_notifyer {
	atomic_t cnt;
	/* cache line break */
	char pad[60];
};

unsigned int odinfs_do_read_delegation(
	struct odinfs_sb_info *sbi, struct mm_struct *mm, unsigned long uaddr,
	unsigned long kaddr, unsigned long bytes, int zero,
	long *odinfs_issued_cnt, struct odinfs_notifyer *odinfs_completed_cnt,
	int wait_hint);

unsigned int odinfs_do_write_delegation(
	struct odinfs_sb_info *sbi, struct mm_struct *mm, unsigned long uaddr,
	unsigned long kaddr, unsigned long bytes, int zero, int flush_cache,
	int sfence, long *odinfs_issued_cnt,
	struct odinfs_notifyer *odinfs_completed_cnt, int wait_hint);

void odinfs_complete_delegation(long *odinfs_issued_cnt,
				struct odinfs_notifyer *odinfs_completed_cnt);

#endif /* __DELEGATION_H_ */
