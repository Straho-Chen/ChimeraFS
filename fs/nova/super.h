#ifndef __SUPER_H
#define __SUPER_H

#include "nova_def.h"
#include <linux/fs.h>

static inline struct nova_super_block *
nova_get_redund_super(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	return (struct nova_super_block *)(sbi->replica_sb_addr);
}

/* If this is part of a read-modify-write of the super block,
 * nova_memunlock_super() before calling!
 */
static inline struct nova_super_block *nova_get_super(struct super_block *sb)
{
	struct nova_sb_info *sbi = NOVA_SB(sb);

	return (struct nova_super_block *)sbi->virt_addr;
}

/* Translate an offset the beginning of the Nova instance to a PMEM address.
 *
 * If this is part of a read-modify-write of the block,
 * nova_memunlock_block() before calling!
 */
static inline void *nova_get_block(struct super_block *sb, u64 block)
{
	struct nova_super_block *ps = nova_get_super(sb);

	return block ? ((void *)ps + block) : NULL;
}

extern struct super_block *nova_read_super(struct super_block *sb, void *data,
					   int silent);
extern int nova_statfs(struct dentry *d, struct kstatfs *buf);
extern int nova_remount(struct super_block *sb, int *flags, char *data);
void *nova_ioremap(struct super_block *sb, phys_addr_t phys_addr, ssize_t size);
extern struct nova_range_node *
nova_alloc_range_node_atomic(struct super_block *sb);
extern struct nova_range_node *nova_alloc_range_node(struct super_block *sb);
extern void nova_free_range_node(struct nova_range_node *node);
extern void nova_update_super_crc(struct super_block *sb);
extern void nova_sync_super(struct super_block *sb);

struct snapshot_info *nova_alloc_snapshot_info(struct super_block *sb);
#endif
