#ifndef __LOG_H
#define __LOG_H

#include "nova_def.h"
#include "balloc.h"

int nova_invalidate_logentry(struct super_block *sb, void *entry,
			     enum nova_entry_type type, unsigned int num_free);
int nova_reassign_logentry(struct super_block *sb, void *entry,
			   enum nova_entry_type type);
int nova_inplace_update_log_entry(struct super_block *sb, struct inode *inode,
				  void *entry,
				  struct nova_log_entry_info *entry_info);
void nova_clear_last_page_tail(struct super_block *sb, struct inode *inode,
			       loff_t newsize);
unsigned int nova_free_old_entry(struct super_block *sb,
				 struct nova_inode_info_header *sih,
				 struct nova_file_write_entry *entry,
				 unsigned long pgoff, unsigned int num_free,
				 bool delete_dead, u64 epoch_id);
int nova_free_inode_log(struct super_block *sb, struct nova_inode *pi,
			struct nova_inode_info_header *sih);
int nova_update_alter_pages(struct super_block *sb, struct nova_inode *pi,
			    u64 curr, u64 alter_curr);
struct nova_file_write_entry *
nova_find_next_entry(struct super_block *sb, struct nova_inode_info_header *sih,
		     pgoff_t pgoff);
int nova_allocate_inode_log_pages(struct super_block *sb,
				  struct nova_inode_info_header *sih,
				  unsigned long num_pages, u64 *new_block,
				  int cpuid,
				  enum nova_alloc_direction from_tail);
int nova_free_contiguous_log_blocks(struct super_block *sb,
				    struct nova_inode_info_header *sih,
				    u64 head);
u64 nova_get_append_head(struct super_block *sb, struct nova_inode *pi,
			 struct nova_inode_info_header *sih, u64 tail,
			 size_t size, int log_id, int thorough_gc,
			 int *extended);
int nova_handle_setattr_operation(struct super_block *sb, struct inode *inode,
				  struct nova_inode *pi, unsigned int ia_valid,
				  struct iattr *attr, u64 epoch_id);
int nova_invalidate_link_change_entry(struct super_block *sb,
				      u64 old_link_change);
int nova_append_link_change_entry(struct super_block *sb, struct nova_inode *pi,
				  struct inode *inode,
				  struct nova_inode_update *update,
				  u64 *old_linkc, u64 epoch_id);
int nova_set_write_entry_updating(struct super_block *sb,
				  struct nova_file_write_entry *entry, int set);
int nova_inplace_update_write_entry(struct super_block *sb, struct inode *inode,
				    struct nova_file_write_entry *entry,
				    struct nova_log_entry_info *entry_info);
int nova_append_mmap_entry(struct super_block *sb, struct nova_inode *pi,
			   struct inode *inode, struct nova_mmap_entry *data,
			   struct nova_inode_update *update,
			   struct vma_item *item);
int nova_append_file_write_entry(struct super_block *sb, struct nova_inode *pi,
				 struct inode *inode,
				 struct nova_file_write_entry *data,
				 struct nova_inode_update *update);
int nova_append_snapshot_info_entry(struct super_block *sb,
				    struct nova_inode *pi,
				    struct nova_inode_info *si,
				    struct snapshot_info *info,
				    struct nova_snapshot_info_entry *data,
				    struct nova_inode_update *update);
int nova_assign_write_entry(struct super_block *sb,
			    struct nova_inode_info_header *sih,
			    struct nova_file_write_entry *entry,
			    struct nova_file_write_entry *entryc, bool free);

void nova_print_curr_log_page(struct super_block *sb, u64 curr);
void nova_print_nova_log(struct super_block *sb,
			 struct nova_inode_info_header *sih);
int nova_get_nova_log_pages(struct super_block *sb,
			    struct nova_inode_info_header *sih,
			    struct nova_inode *pi);
void nova_print_nova_log_pages(struct super_block *sb,
			       struct nova_inode_info_header *sih);

#endif
