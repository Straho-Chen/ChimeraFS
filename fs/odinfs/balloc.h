/*
 * balloc.h
 *
 *  Created on: Nov 26, 2021
 *      Author: dzhou
 */

#ifndef __BALLOC_H_
#define __BALLOC_H_

#include <linux/rbtree_types.h>

/* Range node type */
enum node_type {
	NODE_BLOCK = 1,
	NODE_INODE,
	NODE_DIR,
};

/* A node in the RB tree representing a range of pages */
struct odinfs_range_node {
	struct rb_node node;
	union {
		/* Block, inode */
		struct {
			unsigned long range_low;
			unsigned long range_high;
		};
		/* Dir node */
		struct {
			unsigned long hash;
			void *direntry;
		};
	};
};

int odinfs_find_range_node(struct rb_root *tree, unsigned long key,
			   enum node_type type,
			   struct odinfs_range_node **ret_node);

int odinfs_insert_range_node(struct rb_root *tree,
			     struct odinfs_range_node *new_node,
			     enum node_type type);

void odinfs_destroy_range_node_tree(struct rb_root *tree);

#endif /* BALLOC_H_ */
