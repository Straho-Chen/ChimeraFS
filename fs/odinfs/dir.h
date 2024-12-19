/*
 * dir.h
 *
 *  Created on: Nov 26, 2021
 *      Author: dzhou
 */

#ifndef __DIR_H_
#define __DIR_H_

#include "balloc.h"
#include "odinfs_def.h"

int odinfs_insert_dir_tree(struct super_block *sb,
			   struct odinfs_inode_info_header *sih,
			   const char *name, int namelen,
			   struct odinfs_direntry *direntry);

struct odinfs_direntry *odinfs_find_dentry(struct super_block *sb,
					   struct odinfs_inode *pi,
					   struct inode *inode,
					   const char *name,
					   unsigned long name_len);

static inline void odinfs_delete_dir_tree(struct super_block *sb,
					  struct odinfs_inode_info_header *sih)
{
	odinfs_destroy_range_node_tree(&sih->rb_tree);
}

#endif /* DIR_H_ */
