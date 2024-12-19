#include <linux/bitops.h>
#include "slfs.h"

// we use per cpu (or per socket) allocator for block allocation

int slfs_alloc_block_free_lists(struct slfs_sb_info *sbi)
{
	struct slfs_free_list *free_list;
	int i, j;

	sbi->free_list = kcalloc(sbi->cpus * sbi->sockets,
				 sizeof(struct slfs_free_list), GFP_KERNEL);
	if (!sbi->free_list) {
		slfs_err("alloc free list failed\n");
		return -ENOMEM;
	}
	for (i = 0; i < sbi->cpus; i++) {
		for (j = 0; j < sbi->sockets; j++) {
			free_list = slfs_get_free_list(sbi, i, j);

			// init free block tree
			mt_init(&free_list->block_free_tree);
		}
	}
	return 0;
}

void slfs_delete_block_free_lists(struct slfs_sb_info *sbi)
{
	kfree(sbi->free_list);
	sbi->free_list = NULL;
}

void slfs_init_free_list(struct slfs_sb_info *sbi,
			 struct slfs_free_list *free_list, int cpu, int socket)
{
	// each cpu get the equal capacity
	unsigned long capacity;
	unsigned long size = sbi->block_info[socket].end_block -
			     sbi->block_info[socket].start_block + 1;

	capacity = size / sbi->cpus;

	if (cpu == sbi->cpus - 1) {
		free_list->block_start =
			sbi->block_info[socket].start_block + capacity * cpu;
		free_list->block_end =
			sbi->block_info[socket].start_block + capacity - 1;
	} else {
		free_list->block_start =
			sbi->block_info[socket].start_block + capacity * cpu;

		free_list->block_end = sbi->block_info[socket].end_block;
	}

	// head socket should be reserved some blocks for journal log
	if (cpu == 0 && socket == sbi->head_socket)
		free_list->block_start += sbi->head_reserved_blocks;
}

int slfs_insert_blocktree(struct maple_tree *tree,
			  struct slfs_blocknode *new_node)
{
	// insert new range into free block tree
	int ret = 0;
	ret = mtree_insert_range(tree, new_node->block_low,
				 new_node->block_high, new_node, GFP_KERNEL);
	if (ret) {
		slfs_err("%s maple insert range failed, err code: %d\n",
			 __func__, ret);
	}
	return ret;
}

void slfs_init_blockmap(struct slfs_sb_info *sbi, int recovery)
{
	struct slfs_free_list *free_list;
	struct maple_tree *tree;
	struct slfs_blocknode *blknode;
	int i, j, ret;

	// init per cpu free list
	for (i = 0; i < sbi->cpus; i++) {
		for (j = 0; j < sbi->sockets; j++) {
			// get free list
			free_list = slfs_get_free_list(sbi, i, j);
			// init free list
			slfs_init_free_list(sbi, free_list, i, j);

			// get free block tree
			tree = &free_list->block_free_tree;

			/* For recovery, update these fields later */
			if (recovery == 0) {
				free_list->num_free_blocks =
					free_list->block_end -
					free_list->block_start + 1;

				blknode = slfs_alloc_blocknode();

				if (blknode == NULL)
					BUG();

				blknode->block_low = free_list->block_start;
				blknode->block_high = free_list->block_end;

				ret = slfs_insert_blocktree(tree, blknode);
				if (ret) {
					slfs_err("%s failed\n", __func__);
					slfs_free_blocknode(blknode);
					return;
				}
				free_list->first_node = blknode;
				free_list->last_node = blknode;
				free_list->num_blocknodes = 1;
			}

			slfs_debug(
				"%s: free list, addr: %lx, cpu %d, socket %d,  block "
				"start %lu, end %lu, "
				"%lu free blocks\n",
				__func__, (unsigned long)free_list, i, j,
				free_list->block_start, free_list->block_end,
				free_list->num_free_blocks);
		}
	}
}
