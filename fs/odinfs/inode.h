#ifndef __ODINFS_INODE_H__
#define __ODINFS_INODE_H__

#include <linux/fs.h>

#include "pcpu_rwsem.h"
#include "odinfs_config.h"

struct odinfs_inode_info {
	__u32 i_dir_start_lookup;
	struct list_head i_truncated;
	struct odinfs_inode_info_header header;
	struct inode vfs_inode;

	struct range_lock rlock;
	/* Need cacheline break here? */
#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
	struct pcpu_rwsem rwlock;
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
	struct percpu_rw_semaphore rwlock;
#endif
};

static inline struct odinfs_inode_info *ODINFS_I(struct inode *inode)
{
	return container_of(inode, struct odinfs_inode_info, vfs_inode);
}

extern void odinfs_update_isize(struct inode *inode, struct odinfs_inode *pi);
extern void odinfs_update_nlink(struct inode *inode, struct odinfs_inode *pi);
extern void odinfs_update_time(struct inode *inode, struct odinfs_inode *pi);

extern void odinfs_set_inode_flags(struct inode *inode,
				   struct odinfs_inode *pi);
extern void odinfs_get_inode_flags(struct inode *inode,
				   struct odinfs_inode *pi);
#endif
