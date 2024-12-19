#ifndef _SLFS_H
#define _SLFS_H

#include "slfs_def.h"
#include "mprotect.h"

static inline struct slfs_sb_info *SLFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct slfs_inode_info *SLFS_I(struct inode *inode)
{
	return container_of(inode, struct slfs_inode_info, vfs_inode);
}

static inline struct slfs_super_block *slfs_get_super(struct slfs_sb_info *sbi)
{
	return (struct slfs_super_block *)sbi->virt_addr;
}

static inline struct slfs_free_list *
slfs_get_free_list(struct slfs_sb_info *sbi, int cpu, int socket)
{
	return &sbi->free_list[cpu * sbi->sockets + socket];
}

static inline struct slfs_inode *slfs_get_inode(struct slfs_sb_info *sbi,
						uint64_t ino)
{
	struct slfs_super_block *slfs_sb = slfs_get_super(sbi);
	struct slfs_inode *fst_inode =
		(struct slfs_inode *)((char *)slfs_sb +
				      slfs_sb->s_inode_table_offset);
	return fst_inode + ino;
}

static inline struct slfs_journal *slfs_get_journal(struct slfs_sb_info *sbi)
{
	struct slfs_super_block *slfs_sb = slfs_get_super(sbi);

	return (struct slfs_journal *)((char *)slfs_sb +
				       le64_to_cpu(slfs_sb->s_journal_offset));
}

extern struct kmem_cache *slfs_blocknode_cachep;

static inline struct slfs_blocknode *slfs_alloc_blocknode(void)
{
	struct slfs_blocknode *p;

	p = (struct slfs_blocknode *)kmem_cache_zalloc(slfs_blocknode_cachep,
						       GFP_NOFS);

	return p;
}

static inline void slfs_free_blocknode(struct slfs_blocknode *node)
{
	kmem_cache_free(slfs_blocknode_cachep, node);
}

// balloc.c
int slfs_alloc_block_free_lists(struct slfs_sb_info *sb);
void slfs_delete_block_free_lists(struct slfs_sb_info *sb);
void slfs_init_blockmap(struct slfs_sb_info *sb, int recovery);

// inode.c
extern int slfs_write_inode(struct inode *inode, struct writeback_control *wbc);
extern void slfs_dirty_inode(struct inode *inode, int flags);
extern void slfs_evict_inode(struct inode *inode);

#endif