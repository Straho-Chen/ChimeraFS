/*
 * ODINFS emulated persistence. This file contains code to
 * handle data blocks of various sizes efficiently.
 *
 * Persistent Memory File System
 * Copyright (c) 2012-2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "odinfs.h"
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/slab.h>

int odinfs_setup_blocknode_map(struct super_block *sb)
{
	return 0;
}

void odinfs_save_blocknode_mappings(struct super_block *sb)
{
}

#if 0

#include "odinfs.h"
#include <linux/bitops.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct scan_bitmap {
	unsigned long bitmap_4k_size;
	unsigned long bitmap_2M_size;
	unsigned long bitmap_1G_size;
	unsigned long *bitmap_4k;
	unsigned long *bitmap_2M;
	unsigned long *bitmap_1G;
};

static void odinfs_clear_datablock_inode(struct super_block *sb)
{
	struct odinfs_inode *pi =  odinfs_get_inode(sb, ODINFS_BLOCKNODE_IN0);
	odinfs_transaction_t *trans;

	/* 2 log entry for inode */
	trans = odinfs_new_transaction(sb, MAX_INODE_LENTRIES);
	if (IS_ERR(trans))
		return;
	odinfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	odinfs_memunlock_inode(sb, pi);
	memset(pi, 0, MAX_DATA_PER_LENTRY);
	odinfs_memlock_inode(sb, pi);

	/* commit the transaction */
	odinfs_commit_transaction(sb, trans);
}

static void odinfs_init_blockmap_from_inode(struct super_block *sb)
{
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	struct odinfs_inode *pi =  odinfs_get_inode(sb, ODINFS_BLOCKNODE_IN0);
	struct odinfs_blocknode_lowhigh *p = NULL;
	struct odinfs_blocknode *blknode;
	unsigned long index;
	unsigned long blocknr;
	unsigned long i;
	unsigned long num_blocknode;
	u64 bp;

	num_blocknode = sbi->num_blocknode_allocated;
	sbi->num_blocknode_allocated = 0;
	for (i=0; i<num_blocknode; i++) {
		index = i & 0xFF;
		if (index == 0) {
			/* Find and get new data block */
			blocknr = i >> 8; /* 256 Entries in a block */
			bp = __odinfs_find_data_block(sb, pi, blocknr);
			p = odinfs_get_virt_addr_from_offset(sb, bp);
		}
		ODINFS_ASSERT(p);
		blknode = odinfs_alloc_blocknode(sb);
		if (blknode == NULL)
                	ODINFS_ASSERT(0);
		blknode->block_low = le64_to_cpu(p[index].block_low);
		blknode->block_high = le64_to_cpu(p[index].block_high);
		list_add_tail(&blknode->link, &sbi->block_inuse_head);
	}
}

static bool odinfs_can_skip_full_scan(struct super_block *sb)
{
	struct odinfs_inode *pi =  odinfs_get_inode(sb, ODINFS_BLOCKNODE_IN0);
	struct odinfs_super_block *super = odinfs_get_super(sb);
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	__le64 root;
	unsigned int height, btype;
	unsigned long last_blocknr;

	if (!pi->root)
		return false;

	sbi->num_blocknode_allocated =
		le64_to_cpu(super->s_num_blocknode_allocated);
	sbi->s_inodes_count = le32_to_cpu(super->s_inodes_count);
	sbi->s_free_inodes_count = le32_to_cpu(super->s_free_inodes_count);
	sbi->s_inodes_used_count = le32_to_cpu(super->s_inodes_used_count);
	sbi->s_free_inode_hint = le32_to_cpu(super->s_free_inode_hint);

	odinfs_init_blockmap_from_inode(sb);

	root = pi->root;
	height = pi->height;
	btype = pi->i_blk_type;
	/* pi->i_size can not be zero */
	last_blocknr = (le64_to_cpu(pi->i_size) - 1) >>
					odinfs_inode_blk_shift(pi);

	/* Clearing the datablock inode */
	odinfs_clear_datablock_inode(sb);

	odinfs_free_inode_subtree(sb, root, height, btype, last_blocknr);

	return true;
}


static int odinfs_allocate_datablock_block_inode(odinfs_transaction_t *trans,
	struct super_block *sb, struct odinfs_inode *pi, unsigned long num_blocks)
{
	int errval;
	
	odinfs_memunlock_inode(sb, pi);
	pi->i_mode = 0;
	pi->i_links_count = cpu_to_le16(1);
	pi->i_blk_type = ODINFS_BLOCK_TYPE_4K;
	pi->i_flags = 0;
	pi->height = 0;
	pi->nsocket = odinfs_get_init_nsocket(sb, pi);
	pi->i_dtime = 0; 
	pi->i_size = cpu_to_le64(num_blocks << sb->s_blocksize_bits);
	odinfs_memlock_inode(sb, pi);

	errval = __odinfs_alloc_blocks(trans, sb, pi, 0, num_blocks, false);

	return errval;
}

void odinfs_save_blocknode_mappings(struct super_block *sb)
{
	unsigned long num_blocks, blocknr;
	struct odinfs_inode *pi =  odinfs_get_inode(sb, ODINFS_BLOCKNODE_IN0);
	struct odinfs_blocknode_lowhigh *p;
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	struct list_head *head = &(sbi->block_inuse_head);
	struct odinfs_blocknode *i;
	struct odinfs_super_block *super;
	odinfs_transaction_t *trans;
	u64 bp;
	int j, k;
	int errval;
	
	num_blocks = ((sbi->num_blocknode_allocated * sizeof(struct 
		odinfs_blocknode_lowhigh) - 1) >> sb->s_blocksize_bits) + 1;

	/* 2 log entry for inode, 2 lentry for super-block */
	trans = odinfs_new_transaction(sb, MAX_INODE_LENTRIES + MAX_SB_LENTRIES);
	if (IS_ERR(trans))
		return;

	odinfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	errval = odinfs_allocate_datablock_block_inode(trans, sb, pi, num_blocks);

	if (errval != 0) {
		odinfs_dbg("Error saving the blocknode mappings: %d\n", errval);
		odinfs_abort_transaction(sb, trans);
		return;
	}

	j = 0;
	k = 0;
	p = NULL;
	list_for_each_entry(i, head, link) {
		blocknr = k >> 8;
		if (j == 0) {
			/* Find, get and unlock new data block */
			bp = __odinfs_find_data_block(sb, pi, blocknr);
			p = odinfs_get_virt_addr_from_offset(sb, bp); 
			odinfs_memunlock_block(sb, p);
		}
		p[j].block_low = cpu_to_le64(i->block_low);
		p[j].block_high = cpu_to_le64(i->block_high);
		j++;

		if (j == 256) {
			j = 0;
			/* Lock the data block */
			odinfs_memlock_block(sb, p);
			odinfs_flush_buffer(p, 4096, false);
		}
		
		k++;
	}
	
	/* Lock the block */	
	if (j) {
		odinfs_flush_buffer(p, j << 4, false);
		odinfs_memlock_block(sb, p);	
	}	

	/* 
	 * save the total allocated blocknode mappings 
	 * in super block
	 */
	super = odinfs_get_super(sb);
	odinfs_add_logentry(sb, trans, &super->s_wtime,
			ODINFS_FAST_MOUNT_FIELD_SIZE, LE_DATA);

	odinfs_memunlock_range(sb, &super->s_wtime, ODINFS_FAST_MOUNT_FIELD_SIZE);

	super->s_wtime = cpu_to_le32(ktime_get_seconds());
	super->s_num_blocknode_allocated = 
			cpu_to_le64(sbi->num_blocknode_allocated);
	super->s_inodes_count = cpu_to_le32(sbi->s_inodes_count);
	super->s_free_inodes_count = cpu_to_le32(sbi->s_free_inodes_count);
	super->s_inodes_used_count = cpu_to_le32(sbi->s_inodes_used_count);
	super->s_free_inode_hint = cpu_to_le32(sbi->s_free_inode_hint);

	odinfs_memlock_range(sb, &super->s_wtime, ODINFS_FAST_MOUNT_FIELD_SIZE);
	/* commit the transaction */
	odinfs_commit_transaction(sb, trans);
}

static void odinfs_inode_crawl_recursive(struct super_block *sb,
				struct scan_bitmap *bm, unsigned long block,
				u32 height, u8 btype)
{
	__le64 *node;
	unsigned int i;

	if (height == 0) {
		/* This is the data block */
		if (btype == ODINFS_BLOCK_TYPE_4K) {
			set_bit(block >> PAGE_SHIFT, bm->bitmap_4k);
		} else if (btype == ODINFS_BLOCK_TYPE_2M) {
			set_bit(block >> PAGE_SHIFT_2M, bm->bitmap_2M);
		} else {
			set_bit(block >> PAGE_SHIFT_1G, bm->bitmap_1G);
		}
		return;
	}

	node = odinfs_get_virt_addr_from_offset(sb, block);
	set_bit(block >> PAGE_SHIFT, bm->bitmap_4k);
	for (i = 0; i < (1 << META_BLK_SHIFT); i++) {
		if (node[i] == 0)
			continue;
		odinfs_inode_crawl_recursive(sb, bm,
			le64_to_cpu(node[i]), height - 1, btype);
	}
}

static inline void odinfs_inode_crawl(struct super_block *sb,
				struct scan_bitmap *bm, struct odinfs_inode *pi)
{
	if (pi->root == 0)
		return;
	odinfs_inode_crawl_recursive(sb, bm, le64_to_cpu(pi->root), pi->height,
					pi->i_blk_type);
}

static void odinfs_inode_table_crawl_recursive(struct super_block *sb,
				struct scan_bitmap *bm, unsigned long block,
				u32 height, u32 btype)
{
	__le64 *node;
	unsigned int i;
	struct odinfs_inode *pi;
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	
	node = odinfs_get_virt_addr_from_offset(sb, block);

	if (height == 0) {
		unsigned int inodes_per_block = INODES_PER_BLOCK(btype);
		if (likely(btype == ODINFS_BLOCK_TYPE_2M))
			set_bit(block >> PAGE_SHIFT_2M, bm->bitmap_2M);
		else
			set_bit(block >> PAGE_SHIFT, bm->bitmap_4k);

		sbi->s_inodes_count += inodes_per_block;
		for (i = 0; i < inodes_per_block; i++) {
			pi = (struct odinfs_inode *)((void *)node +
                                                        ODINFS_INODE_SIZE * i);
			if (le16_to_cpu(pi->i_links_count) == 0 &&
                        	(le16_to_cpu(pi->i_mode) == 0 ||
                         	le32_to_cpu(pi->i_dtime))) {
					/* Empty inode */
					continue;
			}
			sbi->s_inodes_used_count++;
			odinfs_inode_crawl(sb, bm, pi);
		}
		return;
	}

	set_bit(block >> PAGE_SHIFT, bm->bitmap_4k);
	for (i = 0; i < (1 << META_BLK_SHIFT); i++) {
		if (node[i] == 0)
			continue;
		odinfs_inode_table_crawl_recursive(sb, bm,
			le64_to_cpu(node[i]), height - 1, btype);
	}
}

static int odinfs_alloc_insert_blocknode_map(struct super_block *sb,
	unsigned long low, unsigned long high)
{
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	struct list_head *head = &(sbi->block_inuse_head);
	struct odinfs_blocknode *i, *next_i;
	struct odinfs_blocknode *free_blocknode= NULL;
	unsigned long num_blocks = 0;
	struct odinfs_blocknode *curr_node;
	int errval = 0;
	bool found = 0;
	unsigned long next_block_low;
	unsigned long new_block_low;
	unsigned long new_block_high;

	//num_blocks = odinfs_get_numblocks(btype);

	new_block_low = low;
	new_block_high = high;
	num_blocks = high - low + 1;

	list_for_each_entry(i, head, link) {
		if (i->link.next == head) {
			next_i = NULL;
			next_block_low = sbi->block_end;
		} else {
			next_i = list_entry(i->link.next, typeof(*i), link);
			next_block_low = next_i->block_low;
		}


		if (new_block_high >= next_block_low) {
			/* Does not fit - skip to next blocknode */
			continue;
		}

		if ((new_block_low == (i->block_high + 1)) &&
			(new_block_high == (next_block_low - 1)))
		{
			/* Fill the gap completely */
			if (next_i) {
				i->block_high = next_i->block_high;
				list_del(&next_i->link);
				free_blocknode = next_i;
			} else {
				i->block_high = new_block_high;
			}
			found = 1;
			break;
		}

		if ((new_block_low == (i->block_high + 1)) &&
			(new_block_high < (next_block_low - 1))) {
			/* Aligns to left */
			i->block_high = new_block_high;
			found = 1;
			break;
		}

		if ((new_block_low > (i->block_high + 1)) &&
			(new_block_high == (next_block_low - 1))) {
			/* Aligns to right */
			if (next_i) {
				/* right node exist */
				next_i->block_low = new_block_low;
			} else {
				/* right node does NOT exist */
				curr_node = odinfs_alloc_blocknode(sb);
				ODINFS_ASSERT(curr_node);
				if (curr_node == NULL) {
					errval = -ENOSPC;
					break;
				}
				curr_node->block_low = new_block_low;
				curr_node->block_high = new_block_high;
				list_add(&curr_node->link, &i->link);
			}
			found = 1;
			break;
		}

		if ((new_block_low > (i->block_high + 1)) &&
			(new_block_high < (next_block_low - 1))) {
			/* Aligns somewhere in the middle */
			curr_node = odinfs_alloc_blocknode(sb);
			ODINFS_ASSERT(curr_node);
			if (curr_node == NULL) {
				errval = -ENOSPC;
				break;
			}
			curr_node->block_low = new_block_low;
			curr_node->block_high = new_block_high;
			list_add(&curr_node->link, &i->link);
			found = 1;
			break;
		}
	}
	
	if (found == 1) {
		sbi->num_free_blocks -= num_blocks;
	}	

	if (free_blocknode)
		odinfs_free_blocknode(sb, free_blocknode);

	if (found == 0) {
		return -ENOSPC;
	}


	return errval;
}

static int __odinfs_build_blocknode_map(struct super_block *sb,
	unsigned long *bitmap, unsigned long bsize, unsigned long scale)
{
	unsigned long next = 1;
	unsigned long low = 0;

	while (1) {
		next = find_next_bit(bitmap, bsize, next);
		if (next == bsize)
			break;
		low = next;
		next = find_next_zero_bit(bitmap, bsize, next);
		if (odinfs_alloc_insert_blocknode_map(sb, low << scale ,
				(next << scale) - 1)) {
			printk("ODINFS: Error could not insert 0x%lx-0x%lx\n",
				low << scale, ((next << scale) - 1));
		}
		if (next == bsize)
			break;
	}
	return 0;
}
	
static void odinfs_build_blocknode_map(struct super_block *sb,
							struct scan_bitmap *bm)
{
	__odinfs_build_blocknode_map(sb, bm->bitmap_4k, bm->bitmap_4k_size * 8,
		PAGE_SHIFT - 12);
	__odinfs_build_blocknode_map(sb, bm->bitmap_2M, bm->bitmap_2M_size * 8,
		PAGE_SHIFT_2M - 12);
	__odinfs_build_blocknode_map(sb, bm->bitmap_1G, bm->bitmap_1G_size * 8,
		PAGE_SHIFT_1G - 12);
}

int odinfs_setup_blocknode_map(struct super_block *sb)
{
	struct odinfs_super_block *super = odinfs_get_super(sb);
	struct odinfs_inode *pi = odinfs_get_inode_table(sb);
	odinfs_journal_t *journal = odinfs_get_journal(sb);
	struct odinfs_sb_info *sbi = ODINFS_SB(sb);
	struct scan_bitmap bm;
	unsigned long initsize = le64_to_cpu(super->s_size);
	bool value = false;
	timing_t start, end;

	/* Always check recovery time */
	if (measure_timing_odinfs == 0)
		ktime_get_ts64(&start);

	ODINFS_START_TIMING(recovery_t, start);

	mutex_init(&sbi->inode_table_mutex);
	sbi->block_start = (unsigned long)0;
	sbi->block_end = ((unsigned long)(initsize) >> PAGE_SHIFT);
	
	value = odinfs_can_skip_full_scan(sb);
	if (value) {
		odinfs_dbg_verbose("ODINFS: Skipping full scan of inodes...\n");
		goto end;
	}

	odinfs_dbg("ODINFS: Performing failure recovery\n");
	bm.bitmap_4k_size = (initsize >> (PAGE_SHIFT + 0x3)) + 1;
	bm.bitmap_2M_size = (initsize >> (PAGE_SHIFT_2M + 0x3)) + 1;
	bm.bitmap_1G_size = (initsize >> (PAGE_SHIFT_1G + 0x3)) + 1;

	/* Alloc memory to hold the block alloc bitmap */
	bm.bitmap_4k = kzalloc(bm.bitmap_4k_size, GFP_KERNEL);
	bm.bitmap_2M = kzalloc(bm.bitmap_2M_size, GFP_KERNEL);
	bm.bitmap_1G = kzalloc(bm.bitmap_1G_size, GFP_KERNEL);

	if (!bm.bitmap_4k || !bm.bitmap_2M || !bm.bitmap_1G)
		goto skip;
	
	/* Clearing the datablock inode */
	odinfs_clear_datablock_inode(sb);

	odinfs_inode_table_crawl_recursive(sb, &bm, le64_to_cpu(pi->root),
						pi->height, pi->i_blk_type);

	/* Reserving tow inodes - Inode 0 and Inode for datablock */
	sbi->s_free_inodes_count = sbi->s_inodes_count -  
		(sbi->s_inodes_used_count + 2);
	
	/* set the block 0 as this is used */
	sbi->s_free_inode_hint = ODINFS_FREE_INODE_HINT_START;

	/* initialize the num_free_blocks to */
	odinfs_init_blockmap(sb, le64_to_cpu(journal->base) + sbi->jsize);

	odinfs_build_blocknode_map(sb, &bm);

skip:
	
	kfree(bm.bitmap_4k);
	kfree(bm.bitmap_2M);
	kfree(bm.bitmap_1G);

end:
	ODINFS_END_TIMING(recovery_t, start);
	if (measure_timing_odinfs == 0) {
		ktime_get_ts64(&end);
		Timingstats_odinfs[recovery_t] +=
			(end.tv_sec - start.tv_sec) * 1000000000 +
			(end.tv_nsec - start.tv_nsec);
	}

	return 0;
}

#endif
