#ifndef __INODE_H
#define __INODE_H

#include "stats.h"
#include "nova_def.h"

extern const struct address_space_operations nova_aops_dax;
int nova_init_inode_inuse_list(struct super_block *sb);
extern int nova_init_inode_table(struct super_block *sb);
int nova_get_alter_inode_address(struct super_block *sb, u64 ino,
				 u64 *alter_pi_addr);
unsigned long nova_get_last_blocknr(struct super_block *sb,
				    struct nova_inode_info_header *sih);
int nova_get_inode_address(struct super_block *sb, u64 ino, int version,
			   u64 *pi_addr, int extendable, int extend_alternate);
int nova_set_blocksize_hint(struct super_block *sb, struct inode *inode,
			    struct nova_inode *pi, loff_t new_size);
extern struct inode *nova_iget(struct super_block *sb, unsigned long ino);
extern void nova_evict_inode(struct inode *inode);
extern int nova_write_inode(struct inode *inode, struct writeback_control *wbc);
extern void nova_dirty_inode(struct inode *inode, int flags);
extern int nova_notify_change(struct mnt_idmap *idmap, struct dentry *dentry,
			      struct iattr *attr);
extern int nova_getattr(struct mnt_idmap *idmap, const struct path *path,
			struct kstat *stat, u32 request_mask,
			unsigned int query_flags);
extern void nova_set_inode_flags(struct inode *inode, struct nova_inode *pi,
				 unsigned int flags);
extern unsigned long nova_find_region(struct inode *inode, loff_t *offset,
				      int hole);
int nova_delete_file_tree(struct super_block *sb,
			  struct nova_inode_info_header *sih,
			  unsigned long start_blocknr,
			  unsigned long last_blocknr, bool delete_nvmm,
			  bool delete_dead, u64 trasn_id);
u64 nova_new_nova_inode(struct super_block *sb, u64 *pi_addr);
extern struct inode *nova_new_vfs_inode(struct mnt_idmap *idmap,
					enum nova_new_inode_type,
					struct inode *dir, u64 pi_addr, u64 ino,
					umode_t mode, size_t size, dev_t rdev,
					const struct qstr *qstr, u64 epoch_id);

#endif
