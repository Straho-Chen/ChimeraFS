/*
 * BRIEF DESCRIPTION
 *
 * File operations for files.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "odinfs.h"
#include "odinfs_stats.h"
#include "xip.h"
#include <asm/mman.h>
#include <linux/falloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/uio.h>

static inline int odinfs_can_set_blocksize_hint(struct odinfs_inode *pi,
						loff_t new_size)
{
	/* Currently, we don't deallocate data blocks till the file is deleted.
   * So no changing blocksize hints once allocation is done. */
	if (le64_to_cpu(pi->root))
		return 0;
	return 1;
}

int odinfs_set_blocksize_hint(struct super_block *sb, struct odinfs_inode *pi,
			      loff_t new_size)
{
	unsigned short block_type;

	if (!odinfs_can_set_blocksize_hint(pi, new_size))
		return 0;

	block_type = ODINFS_DEFAULT_BLOCK_TYPE;

	odinfs_dbg_verbose(
		"Hint: new_size 0x%llx, i_size 0x%llx, root 0x%llx\n", new_size,
		pi->i_size, le64_to_cpu(pi->root));
	odinfs_dbg_verbose("Setting the hint to 0x%x\n", block_type);
	odinfs_memunlock_inode(sb, pi);
	pi->i_blk_type = block_type;
	odinfs_memlock_inode(sb, pi);
	return 0;
}

static long odinfs_fallocate(struct file *file, int mode, loff_t offset,
			     loff_t len)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	long ret = 0;
	unsigned long blocknr, blockoff;
	int num_blocks, blocksize_mask;
	struct odinfs_inode *pi;
	odinfs_transaction_t *trans;
	loff_t new_size;

	/* We only support the FALLOC_FL_KEEP_SIZE mode */
	if (mode & ~FALLOC_FL_KEEP_SIZE)
		return -EOPNOTSUPP;

	if (S_ISDIR(inode->i_mode))
		return -ENODEV;

	inode_lock(inode);

	new_size = len + offset;
	if (!(mode & FALLOC_FL_KEEP_SIZE) && new_size > inode->i_size) {
		ret = inode_newsize_ok(inode, new_size);
		if (ret)
			goto out;
	}

	pi = odinfs_get_inode(sb, inode->i_ino);
	if (!pi) {
		ret = -EACCES;
		goto out;
	}
	trans = odinfs_new_transaction(sb, MAX_INODE_LENTRIES +
						   MAX_METABLOCK_LENTRIES);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}
	odinfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	/* Set the block size hint */
	odinfs_set_blocksize_hint(sb, pi, new_size);

	blocksize_mask = sb->s_blocksize - 1;
	blocknr = offset >> sb->s_blocksize_bits;
	blockoff = offset & blocksize_mask;
	num_blocks = (blockoff + len + blocksize_mask) >> sb->s_blocksize_bits;
	ret = odinfs_alloc_blocks(trans, inode, blocknr, num_blocks, true);

	inode->i_mtime = inode->__i_ctime = current_time(inode);

	odinfs_memunlock_inode(sb, pi);
	if (ret || (mode & FALLOC_FL_KEEP_SIZE)) {
		pi->i_flags |= cpu_to_le32(ODINFS_EOFBLOCKS_FL);
	}

	if (!(mode & FALLOC_FL_KEEP_SIZE) && new_size > inode->i_size) {
		inode->i_size = new_size;
		pi->i_size = cpu_to_le64(inode->i_size);
	}
	pi->i_mtime = cpu_to_le32(inode->i_mtime.tv_sec);
	pi->i_ctime = cpu_to_le32(inode->__i_ctime.tv_sec);
	odinfs_memlock_inode(sb, pi);

	odinfs_commit_transaction(sb, trans);

	odinfs_resize_range_lock(get_range_lock(inode), inode->i_size);

out:
	inode_unlock(inode);
	return ret;
}

static loff_t odinfs_llseek(struct file *file, loff_t offset, int origin)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	int retval;

	if (origin != SEEK_DATA && origin != SEEK_HOLE)
		return generic_file_llseek(file, offset, origin);

	inode_lock(inode);
	switch (origin) {
	case SEEK_DATA:
		retval = odinfs_find_region(inode, &offset, 0);
		if (retval) {
			inode_unlock(inode);
			return retval;
		}
		break;
	case SEEK_HOLE:
		retval = odinfs_find_region(inode, &offset, 1);
		if (retval) {
			inode_unlock(inode);
			return retval;
		}
		break;
	}

	if ((offset < 0 && !(file->f_mode & FMODE_UNSIGNED_OFFSET)) ||
	    offset > inode->i_sb->s_maxbytes) {
		inode_unlock(inode);
		return -EINVAL;
	}

	if (offset != file->f_pos) {
		file->f_pos = offset;
		file->f_version = 0;
	}

	inode_unlock(inode);
	return offset;
}

/* This function is called by both msync() and fsync().
 * TODO: Check if we can avoid calling odinfs_flush_buffer() for fsync. We use
 * movnti to write data to files, so we may want to avoid doing unnecessary
 * odinfs_flush_buffer() on fsync() */
int odinfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	/* Sync from start to end[inclusive] */
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	loff_t isize;

	ODINFS_DEFINE_TIMING_VAR(fsync_time);

	ODINFS_START_TIMING(fsync_t, fsync_time);
	/* if the file is not mmap'ed, there is no need to do clflushes */
	if (mapping_mapped(mapping) == 0)
		goto persist;

	end += 1; /* end is inclusive. We like our indices normal please ! */

	isize = i_size_read(inode);

	if ((unsigned long)end > (unsigned long)isize)
		end = isize;
	if (!isize || (start >= end)) {
		odinfs_dbg_verbose("[%s:%d] : (ERR) isize(%llx), start(%llx),"
				   " end(%llx)\n",
				   __func__, __LINE__, isize, start, end);
		ODINFS_END_TIMING(fsync_t, fsync_time);
		return -ENODATA;
	}

	/* Align start and end to cacheline boundaries */
	start = start & CACHELINE_MASK;
	end = CACHELINE_ALIGN(end);
	do {
		sector_t block = 0;
		void *xip_mem;
		pgoff_t pgoff;
		loff_t offset;
		unsigned long nr_flush_bytes;

		pgoff = start >> PAGE_SHIFT;
		offset = start & ~PAGE_MASK;

		nr_flush_bytes = PAGE_SIZE - offset;
		if (nr_flush_bytes > (end - start))
			nr_flush_bytes = end - start;

		block = odinfs_find_data_block(inode, (sector_t)pgoff);

		if (block) {
			xip_mem = odinfs_get_virt_addr_from_offset(inode->i_sb,
								   block);
			/* flush the range */
			increase_fsync_pages_count();
			odinfs_flush_buffer(xip_mem + offset, nr_flush_bytes,
					    0);
		} else {
			/* sparse files could have such holes */
			odinfs_dbg_verbose("[%s:%d] : start(%llx), end(%llx),"
					   " pgoff(%lx)\n",
					   __func__, __LINE__, start, end,
					   pgoff);
			break;
		}

		start += nr_flush_bytes;
	} while (start < end);
persist:
	PERSISTENT_MARK();
	PERSISTENT_BARRIER();
	ODINFS_END_TIMING(fsync_t, fsync_time);
	return 0;
}

/* This callback is called when a file is closed */
static int odinfs_flush(struct file *file, fl_owner_t id)
{
	int ret = 0;
	/* if the file was opened for writing, make it persistent.
   * TODO: Should we be more smart to check if the file was modified? */
	if (file->f_mode & FMODE_WRITE) {
		PERSISTENT_MARK();
		PERSISTENT_BARRIER();
	}

	return ret;
}

const struct file_operations odinfs_xip_file_operations = {
	.llseek = odinfs_llseek,
	.read = odinfs_xip_file_read,
	.write = odinfs_xip_file_write,
	//	.aio_read		= xip_file_aio_read,
	//	.aio_write		= xip_file_aio_write,
	//	.read_iter		= generic_file_read_iter,
	//	.write_iter		= generic_file_write_iter,
	.mmap = odinfs_xip_file_mmap,
	.open = generic_file_open,
	.fsync = odinfs_fsync,
	.flush = odinfs_flush,
	//	.get_unmapped_area	= odinfs_get_unmapped_area,
	.unlocked_ioctl = odinfs_ioctl,
	.fallocate = odinfs_fallocate,
#ifdef CONFIG_COMPAT
	.compat_ioctl = odinfs_compat_ioctl,
#endif
};

const struct inode_operations odinfs_file_inode_operations = {
	.setattr = odinfs_notify_change,
	.getattr = odinfs_getattr,
	.get_acl = NULL,
};
