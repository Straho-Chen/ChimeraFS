#ifndef __RWSEM_H_
#define __RWSEM_H_

#include <linux/percpu-rwsem.h>

#include "odinfs_config.h"

#include "pcpu_rwsem.h"

#define odinfs_pinode_rwsem(i) (&((i)->rwlock))

#define odinfs_inode_rwsem(i) (odinfs_pinode_rwsem(ODINFS_I((i))))

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_inode_rwsem_down_read(i) (inode_lock_shared(i))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_inode_rwsem_down_read(i) \
	(pcpu_rwsem_down_read(odinfs_inode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
#define odinfs_inode_rwsem_down_read(i) \
	(percpu_down_read(odinfs_inode_rwsem(i)))
#endif

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_inode_rwsem_up_read(i) (inode_unlock_shared(i))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_inode_rwsem_up_read(i) \
	(pcpu_rwsem_up_read(odinfs_inode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
#define odinfs_inode_rwsem_up_read(i) (percpu_up_read(odinfs_inode_rwsem(i)))
#endif

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_inode_rwsem_down_write(i) (inode_lock(i))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_inode_rwsem_down_write(i) \
	(pcpu_rwsem_down_write(odinfs_inode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
#define odinfs_inode_rwsem_down_write(i) \
	(percpu_down_write(odinfs_inode_rwsem(i)))
#endif

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_inode_rwsem_up_write(i) (inode_unlock(i))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_inode_rwsem_up_write(i) \
	(pcpu_rwsem_up_write(odinfs_inode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
#define odinfs_inode_rwsem_up_write(i) (percpu_up_write(odinfs_inode_rwsem(i)))
#endif

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_pinode_rwsem_init(i)
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_pinode_rwsem_init(i) \
	(pcpu_rwsem_init_rwsem(odinfs_pinode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
#define odinfs_pinode_rwsem_init(i) (percpu_init_rwsem(odinfs_pinode_rwsem(i)))
#endif

#if ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_STOCK
#define odinfs_pinode_rwsem_free(i)
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_MAX_PERCPU
#define odinfs_pinode_rwsem_free(i) \
	(pcpu_rwsem_free_rwsem(odinfs_pinode_rwsem(i)))
#elif ODINFS_INODE_LOCK == ODINFS_INODE_LOCK_PERCPU
//#define odinfs_pinode_rwsem_free(i) (percpu_free_rwsem(odinfs_pinode_rwsem(i)))
#define odinfs_pinode_rwsem_free(i)
#endif

#endif /* __RWSEM_H_ */
