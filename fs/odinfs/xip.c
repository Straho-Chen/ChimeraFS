/*
 * BRIEF DESCRIPTION
 *
 * XIP operations.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <asm/cpufeature.h>
#include <asm/pgtable.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include "delegation.h"
#include "odinfs.h"
#include "odinfs_config.h"
#include "odinfs_stats.h"
#include "xip.h"
#include "range_lock.h"
#include "rwsem.h"

static ssize_t do_xip_mapping_read(struct address_space *mapping,
				   struct file_ra_state *_ra, struct file *filp,
				   char __user *buf, size_t len, loff_t *ppos)
{
	struct inode *inode = mapping->host;
	pgoff_t index;
	unsigned long strip_offset, page_offset;
	loff_t isize, pos;
	size_t copied = 0, error = 0;

	struct super_block *sb = inode->i_sb;
	struct odinfs_inode *pi = odinfs_get_inode(sb, inode->i_ino);
	int blocks_per_strip = odinfs_get_numblocks(pi->i_blk_type);

	long odinfs_issued_cnt[ODINFS_MAX_SOCKET];
	struct odinfs_notifyer odinfs_completed_cnt[ODINFS_MAX_SOCKET];

#if ODINFS_DELEGATION_ENABLE
	int cond_cnt = 0;
#endif

#if ODINFS_FINE_GRAINED_LOCK
	int range_locked = 0;
	allocate_rlock_info();
#endif

	ODINFS_DEFINE_TIMING_VAR(get_block_time);

#if ODINFS_DELEGATION_ENABLE
	ODINFS_DEFINE_TIMING_VAR(fini_delegation_time);
	ODINFS_DEFINE_TIMING_VAR(do_delegation_time);
#endif

	memset(odinfs_issued_cnt, 0, sizeof(long) * ODINFS_MAX_SOCKET);
	memset(odinfs_completed_cnt, 0,
	       sizeof(struct odinfs_notifyer) * ODINFS_MAX_SOCKET);

	pos = *ppos;

#if ODINFS_FINE_GRAINED_LOCK
	odinfs_inode_rwsem_down_read(inode);
#else /* if ODINFS_FINE_GRAINED_LOCK */
	inode_lock_shared(inode);
#endif

	isize = i_size_read(inode);
	if (!isize || pos >= isize)
		goto out;

#if ODINFS_FINE_GRAINED_LOCK
	assign_rlock_info_read(isize);
	RANGE_READ_LOCK(get_range_lock(inode), get_start_range(),
			get_size_or_end_range());
	range_locked = 1;
#endif

	do {
		/* nr is the maximum number of bytes to copy from this page */
		long nr, left;
		void *xip_mem;
		unsigned long xip_pfn;
		int zero = 0;

		index = pos >> PAGE_SHIFT;
		strip_offset = pos & (blocks_per_strip * PAGE_SIZE - 1);
		page_offset = pos & (~PAGE_MASK);

		/* nr: number of bytes for pos to reach the end of the strip */
		nr = (sb->s_blocksize * blocks_per_strip) - strip_offset;

		/* now consider pos + nr will exceed isize */
		if (pos + nr > isize)
			nr = (isize - pos);

		/* now consider the size of user buffer */
		if (nr > len - copied)
			nr = len - copied;

		if (nr <= 0) {
			goto out;
		}

		ODINFS_START_TIMING(get_block_r_t, get_block_time);
		error = odinfs_get_xip_mem(mapping, index, 0, &xip_mem,
					   &xip_pfn);
		ODINFS_END_TIMING(get_block_r_t, get_block_time);

		if (unlikely(error)) {
			if (error == -ENODATA) {
				/* sparse */
				zero = 1;
			} else
				goto out;
		}

		/*
		 * If users can be writing to this page using arbitrary
		 * virtual addresses, take care about potential aliasing
		 * before reading the page on the kernel side.
		 */
		if (mapping_writably_mapped(mapping))
			/* address based flush */;

#if ODINFS_DELEGATION_ENABLE

		/*
		 * do not delegate if the size is less than
		 * ODINFS_READ_DELEGATION_LIMIT bytes
		 */
		if (nr < ODINFS_READ_DELEGATION_LIMIT) {
			if (!zero)
				left = __copy_to_user(buf + copied,
						      xip_mem + page_offset,
						      nr);
			else
				left = __clear_user(buf + copied, nr);
		} else {
			ODINFS_START_TIMING(do_delegation_r_t,
					    do_delegation_time);

			left = odinfs_do_read_delegation(
				ODINFS_SB(inode->i_sb), current->mm,
				(unsigned long)(char *)buf + copied,
				(unsigned long)xip_mem + page_offset, nr, zero,
				odinfs_issued_cnt, odinfs_completed_cnt,
				len >= ODINFS_READ_WAIT_THRESHOLD);

			ODINFS_END_TIMING(do_delegation_r_t,
					  do_delegation_time);
		}

#else
		/*
		 * Ok, we have the mem, so now we can copy it to user space...
		 *
		 * The actor routine returns how many bytes were actually used..
		 * NOTE! This may not be the same as how much of a user buffer
		 * we filled up (we may be padding etc), so we can only update
		 * "pos" here (the actor routine has to update the user buffer
		 * pointers and the remaining count).
		 */
		if (!zero)
			left = __copy_to_user(buf + copied,
					      xip_mem + page_offset, nr);
		else
			left = __clear_user(buf + copied, nr);
#endif

		odinfs_dbg_strip("read pos: %lld, count: %ld, copied: %ld\n",
				 pos, len - copied, nr - left);

		if (left) {
			error = -EFAULT;
			goto out;
		}

		copied += (nr - left);
		pos += (nr - left);
#if ODINFS_DELEGATION_ENABLE
		cond_cnt++;
		if (cond_cnt >= ODINFS_APP_RING_BUFFER_CHECK_COUNT) {
			cond_cnt = 0;
			if (need_resched())
				cond_resched();
		}
#else
		if (need_resched())
			cond_resched();
#endif
	} while (copied < len);

out:

#if ODINFS_DELEGATION_ENABLE
	ODINFS_START_TIMING(fini_delegation_r_t, fini_delegation_time);
	odinfs_complete_delegation(odinfs_issued_cnt, odinfs_completed_cnt);
	ODINFS_END_TIMING(fini_delegation_r_t, fini_delegation_time);
#endif

	if (filp)
		file_accessed(filp);

#if ODINFS_FINE_GRAINED_LOCK
	if (range_locked) {
		RANGE_READ_UNLOCK(get_range_lock(inode), get_start_range(),
				  get_size_or_end_range());
	}

	odinfs_inode_rwsem_up_read(inode);
#else
	inode_unlock_shared(inode);
#endif

	*ppos = pos;

	return (copied ? copied : error);
}

ssize_t xip_file_read(struct file *filp, char __user *buf, size_t len,
		      loff_t *ppos)
{
	/* if (!access_ok(VERIFY_WRITE, buf, len)) */
	if (!access_ok(buf, len))
		return -EFAULT;

	return do_xip_mapping_read(filp->f_mapping, &filp->f_ra, filp, buf, len,
				   ppos);
}

/*
 * Wrappers. We need to use the rcu read lock to avoid
 * concurrent truncate operation. No problem for write because we held
 * i_mutex.
 */
ssize_t odinfs_xip_file_read(struct file *filp, char __user *buf, size_t len,
			     loff_t *ppos)
{
	ssize_t res;
	ODINFS_DEFINE_TIMING_VAR(xip_read_time);

	ODINFS_START_TIMING(xip_read_t, xip_read_time);
	call_xip_file_read();
	ODINFS_END_TIMING(xip_read_t, xip_read_time);
	return res;
}

static inline void odinfs_flush_edge_cachelines(loff_t pos, ssize_t len,
						void *start_addr)
{
	if (unlikely(pos & 0x7))
		odinfs_flush_buffer(start_addr, 1, false);
	if (unlikely(((pos + len) & 0x7) &&
		     ((pos & (CACHELINE_SIZE - 1)) !=
		      ((pos + len) & (CACHELINE_SIZE - 1)))))
		odinfs_flush_buffer(start_addr + len, 1, false);
}

static inline size_t memcpy_to_nvmm(char *kmem, loff_t offset,
				    const char __user *buf, size_t bytes)
{
	size_t copied;

	if (support_clwb_odinfs) {
		copied = bytes - __copy_from_user(kmem + offset, buf, bytes);
		odinfs_flush_buffer(kmem + offset, copied, 0);
	} else {
		copied = bytes - __copy_from_user_inatomic_nocache(
					 kmem + offset, buf, bytes);
	}

	return copied;
}

static ssize_t __odinfs_xip_file_write(
	struct address_space *mapping, const char __user *buf, size_t count,
	loff_t pos, loff_t *ppos, long *odinfs_issued_cnt,
	struct odinfs_notifyer *odinfs_completed_cnt, size_t len)
{
	struct inode *inode = mapping->host;
	struct super_block *sb = inode->i_sb;
	long status = 0;
	size_t bytes;
	ssize_t written = 0;
	struct odinfs_inode *pi;

	int blocks_per_strip = 0;

#if ODINFS_DELEGATION_ENABLE
	int cond_cnt = 0;
#endif

	ODINFS_DEFINE_TIMING_VAR(write_time);
	ODINFS_DEFINE_TIMING_VAR(get_block_time);
	ODINFS_DEFINE_TIMING_VAR(memcpy_time);

#if ODINFS_DELEGATION_ENABLE
	ODINFS_DEFINE_TIMING_VAR(do_delegation_time);
#endif

	ODINFS_START_TIMING(internal_write_t, write_time);
	pi = odinfs_get_inode(sb, inode->i_ino);

	blocks_per_strip = odinfs_get_numblocks(pi->i_blk_type);

	do {
		long index, strip_offset, page_offset;
		size_t copied;
		void *xmem;
		unsigned long xpfn;
#if ODINFS_DELEGATION_ENABLE
		int left;
#endif

		page_offset = pos & (sb->s_blocksize - 1);

		index = pos >> sb->s_blocksize_bits;
		strip_offset = pos & (sb->s_blocksize * blocks_per_strip - 1);

		/* bytes: number of bytes for pos to reach the end of the strip */
		bytes = (sb->s_blocksize * blocks_per_strip) - strip_offset;

		/* modify bytes if it is larger than count */
		if (bytes > count)
			bytes = count;

		ODINFS_START_TIMING(get_block_w_t, get_block_time);
		status = odinfs_get_xip_mem(mapping, index, 1, &xmem, &xpfn);
		ODINFS_END_TIMING(get_block_w_t, get_block_time);

		if (status)
			break;

#if ODINFS_DELEGATION_ENABLE

		/* do not delegate if bytes is less than ODINFS_WRITE_DELEGATION_LIMIT */
		if (bytes < ODINFS_WRITE_DELEGATION_LIMIT) {
			ODINFS_START_TIMING(memcpy_w_t, memcpy_time);

			odinfs_xip_mem_protect(sb, xmem + page_offset, bytes,
					       1);
			copied = memcpy_to_nvmm((char *)xmem, page_offset, buf,
						bytes);
			odinfs_xip_mem_protect(sb, xmem + page_offset, bytes,
					       0);

			ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
			/*
			 * if start or end dest address is not 8 byte aligned,
			 * __copy_from_user_inatomic_nocache uses cacheable instructions
			 * (instead of movnti) to write. So flush those cachelines.
			 */
			odinfs_flush_edge_cachelines(pos, copied,
						     xmem + page_offset);
		} else {
			ODINFS_START_TIMING(do_delegation_w_t,
					    do_delegation_time);
			left = odinfs_do_write_delegation(
				ODINFS_SB(sb), current->mm, (unsigned long)buf,
				(unsigned long)xmem + page_offset, bytes, 0, 1,
				0, odinfs_issued_cnt, odinfs_completed_cnt,
				len >= ODINFS_WRITE_WAIT_THRESHOLD);
			ODINFS_END_TIMING(do_delegation_w_t,
					  do_delegation_time);

			copied = bytes - left;
		}

#else
		ODINFS_START_TIMING(memcpy_w_t, memcpy_time);

		odinfs_xip_mem_protect(sb, xmem + page_offset, bytes, 1);
		copied = memcpy_to_nvmm((char *)xmem, page_offset, buf, bytes);
		odinfs_xip_mem_protect(sb, xmem + page_offset, bytes, 0);

		/*
		 * if start or end dest address is not 8 byte aligned,
		 * __copy_from_user_inatomic_nocache uses cacheable instructions
		 * (instead of movnti) to write. So flush those cachelines.
		 */
		odinfs_flush_edge_cachelines(pos, copied, xmem + page_offset);

		ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
#endif

		odinfs_dbg_strip("write pos: %lld, count: %ld, copied: %ld\n",
				 pos, count, copied);

		if (likely(copied > 0)) {
			status = copied;

			if (status >= 0) {
				written += status;
				count -= status;
				pos += status;
				buf += status;
			}
		}
		if (unlikely(copied != bytes))
			if (status >= 0)
				status = -EFAULT;
		if (status < 0)
			break;

#if ODINFS_DELEGATION_ENABLE
		cond_cnt++;
		if (cond_cnt >= ODINFS_APP_RING_BUFFER_CHECK_COUNT) {
			cond_cnt = 0;
			if (need_resched())
				cond_resched();
		}
#else
		if (need_resched())
			cond_resched();
#endif
	} while (count);
	*ppos = pos;
	/*
	 * No need to use i_size_read() here, the i_size
	 * cannot change under us because we hold i_mutex.
	 */
	if (pos > inode->i_size) {
		i_size_write(inode, pos);
		odinfs_update_isize(inode, pi);
		odinfs_resize_range_lock(get_range_lock(inode), pos);
	}

	ODINFS_END_TIMING(internal_write_t, write_time);
	return written ? written : status;
}

/* optimized path for file write that doesn't require a transaction. In this
 * path we don't need to allocate any new data blocks. So the only meta-data
 * modified in path is inode's i_size, i_ctime, and i_mtime fields */
static ssize_t
odinfs_file_write_fast(struct super_block *sb, struct inode *inode,
		       struct odinfs_inode *pi, const char __user *buf,
		       size_t count, loff_t pos, loff_t *ppos, u64 block,
		       long *odinfs_issued_cnt,
		       struct odinfs_notifyer *odinfs_completed_cnt, size_t len)
{
	void *xmem = odinfs_get_virt_addr_from_offset(sb, block);
	size_t copied, ret = 0, offset;
	// size_t old_pos = pos;
#if ODINFS_DELEGATION_ENABLE
	unsigned int left;
	ODINFS_DEFINE_TIMING_VAR(do_delegation_time);
#endif

	ODINFS_DEFINE_TIMING_VAR(memcpy_time);

	offset = pos & (sb->s_blocksize - 1);

#if ODINFS_DELEGATION_ENABLE

	/* do not delegate if the size is less than ODINFS_WRITE_DELEGATION_LIMIT */
	if (count < ODINFS_WRITE_DELEGATION_LIMIT) {
		ODINFS_START_TIMING(memcpy_w_t, memcpy_time);

		odinfs_xip_mem_protect(sb, xmem + offset, count, 1);
		copied = memcpy_to_nvmm((char *)xmem, offset, buf, count);
		odinfs_xip_mem_protect(sb, xmem + offset, count, 0);
		odinfs_flush_edge_cachelines(pos, copied, xmem + offset);

		ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
	} else {
		ODINFS_START_TIMING(do_delegation_w_t, do_delegation_time);

		left = odinfs_do_write_delegation(
			ODINFS_SB(sb), current->mm, (unsigned long)buf,
			(unsigned long)xmem + offset, count, 0, 1, 0,
			odinfs_issued_cnt, odinfs_completed_cnt,
			len >= ODINFS_WRITE_WAIT_THRESHOLD);
		copied = count - left;

		ODINFS_END_TIMING(do_delegation_w_t, do_delegation_time);
	}
#else
	ODINFS_START_TIMING(memcpy_w_t, memcpy_time);

	odinfs_xip_mem_protect(sb, xmem + offset, count, 1);
	copied = memcpy_to_nvmm((char *)xmem, offset, buf, count);
	odinfs_xip_mem_protect(sb, xmem + offset, count, 0);
	odinfs_flush_edge_cachelines(pos, copied, xmem + offset);
	ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
#endif

	if (likely(copied > 0)) {
		pos += copied;
		ret = copied;
	}
	if (unlikely(copied != count && copied == 0))
		ret = -EFAULT;
	*ppos = pos;
	inode->__i_ctime = inode->i_mtime = current_time(inode);
	if (pos > inode->i_size) {
		PERSISTENT_MARK();
		i_size_write(inode, pos);
		PERSISTENT_BARRIER();
		odinfs_memunlock_inode(sb, pi);
		odinfs_update_time_and_size(inode, pi);
		odinfs_memlock_inode(sb, pi);

	} else {
		u64 c_m_time;
		/*
		 * update c_time and m_time atomically. We don't need to make the data
		 * persistent because the expectation is that the close() or an explicit
		 * fsync will do that.
		 */
		c_m_time = (inode->__i_ctime.tv_sec & 0xFFFFFFFF);
		c_m_time = c_m_time | (c_m_time << 32);
		odinfs_memunlock_inode(sb, pi);
		odinfs_memcpy_atomic(&pi->i_ctime, &c_m_time, 8);
		odinfs_memlock_inode(sb, pi);
	}
	odinfs_flush_buffer(pi, 1, false);
	return ret;
}

/*
 * blk_off is used in different ways depending on whether the edge block is
 * at the beginning or end of the write. If it is at the beginning, we zero from
 * start-of-block to 'blk_off'. If it is the end block, we zero from 'blk_off'
 * to end-of-block
 */
static inline void
odinfs_clear_edge_blk(struct super_block *sb, struct odinfs_inode *pi,
		      bool new_blk, unsigned long block, size_t blk_off,
		      bool is_end_blk, long *odinfs_issued_cnt,
		      struct odinfs_notifyer *odinfs_completed_cnt, size_t len)
{
	void *ptr;
	size_t count;
	unsigned long blknr;

#if ODINFS_DELEGATION_ENABLE
	ODINFS_DEFINE_TIMING_VAR(do_delegation_time);
#endif

	ODINFS_DEFINE_TIMING_VAR(memcpy_time);

	if (new_blk) {
		blknr = block >>
			(odinfs_inode_blk_shift(pi) - sb->s_blocksize_bits);
		ptr = odinfs_get_virt_addr_from_offset(
			sb, __odinfs_find_data_block(sb, pi, blknr));
		if (ptr != NULL) {
			if (is_end_blk) {
				ptr = ptr + blk_off - (blk_off % 8);
				count = odinfs_inode_blk_size(pi) - blk_off +
					(blk_off % 8);
			} else
				count = blk_off + (8 - (blk_off % 8));
#if ODINFS_DELEGATION_ENABLE
			if (count > ODINFS_WRITE_DELEGATION_LIMIT) {
				ODINFS_START_TIMING(do_delegation_w_t,
						    do_delegation_time);
				odinfs_do_write_delegation(
					ODINFS_SB(sb), NULL, 0,
					(unsigned long)ptr, count, 1, 1, 0,
					odinfs_issued_cnt, odinfs_completed_cnt,
					len >= ODINFS_WRITE_WAIT_THRESHOLD);
				ODINFS_END_TIMING(do_delegation_w_t,
						  do_delegation_time);
			} else {
				ODINFS_START_TIMING(memcpy_w_t, memcpy_time);
				odinfs_memunlock_range(
					sb, ptr, odinfs_inode_blk_size(pi));
				memset_nt(ptr, 0, count);
				odinfs_memlock_range(sb, ptr,
						     odinfs_inode_blk_size(pi));
				ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
			}
#else
			ODINFS_START_TIMING(memcpy_w_t, memcpy_time);
			odinfs_memunlock_range(sb, ptr,
					       odinfs_inode_blk_size(pi));
			memset_nt(ptr, 0, count);
			odinfs_memlock_range(sb, ptr,
					     odinfs_inode_blk_size(pi));
			ODINFS_END_TIMING(memcpy_w_t, memcpy_time);
#endif
		}
	}
}

ssize_t odinfs_xip_file_write(struct file *filp, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct super_block *sb = inode->i_sb;
	odinfs_transaction_t *trans;
	struct odinfs_inode *pi;
	ssize_t written = 0;
	loff_t pos;
	u64 block;
	bool new_sblk = false, new_eblk = false;
	size_t count, offset, eblk_offset, ret;
	unsigned long start_blk, end_blk, num_blocks, max_logentries;
	bool same_block;

	long odinfs_issued_cnt[ODINFS_MAX_SOCKET];
	struct odinfs_notifyer odinfs_completed_cnt[ODINFS_MAX_SOCKET];

	bool fast_path = false;

#if ODINFS_FINE_GRAINED_LOCK
	allocate_rlock_info();
#endif

	ODINFS_DEFINE_TIMING_VAR(xip_write_time);
	ODINFS_DEFINE_TIMING_VAR(xip_write_fast_time);

#if ODINFS_DELEGATION_ENABLE
	ODINFS_DEFINE_TIMING_VAR(fini_delegation_time);
#endif

	ODINFS_START_TIMING(xip_write_t, xip_write_time);

	memset(odinfs_issued_cnt, 0, sizeof(long) * ODINFS_MAX_SOCKET);
	memset(odinfs_completed_cnt, 0,
	       sizeof(struct odinfs_notifyer) * ODINFS_MAX_SOCKET);

	if (!access_ok(buf, len)) {
		ret = -EFAULT;
		goto out;
	}

	sb_start_write(inode->i_sb);

	pos = *ppos;
	count = len;
	if (count == 0) {
		ret = 0;
		goto out_nolock;
	}

	pi = odinfs_get_inode(sb, inode->i_ino);

#if ODINFS_FINE_GRAINED_LOCK
	odinfs_inode_rwsem_down_read(inode);
#else
	inode_lock(inode);
#endif

	offset = pos & (sb->s_blocksize - 1);
	num_blocks = ((count + offset - 1) >> sb->s_blocksize_bits) + 1;
	/* offset in the actual block size block */
	offset = pos & (odinfs_inode_blk_size(pi) - 1);
	start_blk = pos >> sb->s_blocksize_bits;
	end_blk = start_blk + num_blocks - 1;

	block = odinfs_find_data_block(inode, start_blk);

	/* Referring to the inode's block size, not 4K */

	/*
     * We still reuse the fast_path testing mechanism in odinfs as the first
     * fast_path test to avoid performnance surprise.
     */
	same_block =
		(((count + offset - 1) >> odinfs_inode_blk_shift(pi)) == 0) ?
			1 :
			0;

	fast_path = block && same_block;

	/*
     * The odinfs_need_alloc_blocks function has bugs, come back to debug it later,
     * if time allows
     */
#if 0
    /*
     * Now, we test whether this write requires alloc blocks. If not, then
     * we can handle it through the fast_path
     */
    if (block && !fast_path) {
        fast_path = (odinfs_need_alloc_blocks(inode->i_sb, pi, start_blk,
                num_blocks) == 0);
    }
#endif

	if (fast_path) {
		ODINFS_START_TIMING(xip_write_fast_t, xip_write_fast_time);

#if ODINFS_FINE_GRAINED_LOCK
		assign_rlock_info_write();

		RANGE_WRITE_LOCK(get_range_lock(inode), get_start_range(),
				 get_size_or_end_range());
#endif

		ret = odinfs_file_write_fast(sb, inode, pi, buf, count, pos,
					     ppos, block, odinfs_issued_cnt,
					     odinfs_completed_cnt, len);
		ODINFS_END_TIMING(xip_write_fast_t, xip_write_fast_time);

#if ODINFS_DELEGATION_ENABLE
		ODINFS_START_TIMING(fini_delegation_w_t, fini_delegation_time);
		odinfs_complete_delegation(odinfs_issued_cnt,
					   odinfs_completed_cnt);
		ODINFS_END_TIMING(fini_delegation_w_t, fini_delegation_time);
#endif

		goto out;
	}

#if ODINFS_FINE_GRAINED_LOCK

	odinfs_inode_rwsem_up_read(inode);
	/* Acquire the write lock to enter the slow path */

	/*
     * It is bad that I cannot upgrade a read lock to a write lock. However,
     * in this case, we don't need to worry about the race condition between
     * releasing the read lock and acquiring the write lock. Acquire the writer
     * lock is the most conservative approach. A race condition may make us can
     * proceed with the fast path so we are not achieving the optimal
     * performance. But correctness wise, we are fine.
     */
	odinfs_inode_rwsem_down_write(inode);
#endif

	max_logentries = num_blocks / MAX_PTRS_PER_LENTRY + 2;
	if (max_logentries > MAX_METABLOCK_LENTRIES)
		max_logentries = MAX_METABLOCK_LENTRIES;

	trans = odinfs_new_transaction(sb, MAX_INODE_LENTRIES + max_logentries);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}
	odinfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY, LE_DATA);

	ret = file_remove_privs(filp);
	if (ret) {
		odinfs_abort_transaction(sb, trans);
		goto out;
	}

	inode->__i_ctime = inode->i_mtime = current_time(inode);
	odinfs_update_time(inode, pi);

	/*
	 * We avoid zeroing the alloc'd range, which is going to be overwritten
	 * by this system call anyway
	 */
	if (offset != 0) {
		if (odinfs_find_data_block(inode, start_blk) == 0)
			new_sblk = true;
	}

	eblk_offset = (pos + count) & (odinfs_inode_blk_size(pi) - 1);
	if ((eblk_offset != 0) && (odinfs_find_data_block(inode, end_blk) == 0))
		new_eblk = true;

	/* don't zero-out the allocated blocks */
	odinfs_alloc_blocks(trans, inode, start_blk, num_blocks, false);

	/* now zero out the edge blocks which will be partially written */
	odinfs_clear_edge_blk(sb, pi, new_sblk, start_blk, offset, false,
			      odinfs_issued_cnt, odinfs_completed_cnt, len);
	odinfs_clear_edge_blk(sb, pi, new_eblk, end_blk, eblk_offset, true,
			      odinfs_issued_cnt, odinfs_completed_cnt, len);

	written = __odinfs_xip_file_write(mapping, buf, count, pos, ppos,
					  odinfs_issued_cnt,
					  odinfs_completed_cnt, len);

	/* should be wait here, otherwise the consistency is not guaranteed. */
#if ODINFS_DELEGATION_ENABLE
	ODINFS_START_TIMING(fini_delegation_w_t, fini_delegation_time);
	odinfs_complete_delegation(odinfs_issued_cnt, odinfs_completed_cnt);
	ODINFS_END_TIMING(fini_delegation_w_t, fini_delegation_time);
#endif

	if (written < 0 || written != count)
		odinfs_dbg_verbose(
			"write incomplete/failed: written %ld len %ld"
			" pos %llx start_blk %lx num_blocks %lx\n",
			written, count, pos, start_blk, num_blocks);

	odinfs_commit_transaction(sb, trans);
	ret = written;

out:
#if ODINFS_FINE_GRAINED_LOCK
	if (fast_path) {
		RANGE_WRITE_UNLOCK(get_range_lock(inode), get_start_range(),
				   get_size_or_end_range());
		odinfs_inode_rwsem_up_read(inode);
	} else {
		odinfs_inode_rwsem_up_write(inode);
	}
#else
	inode_unlock(inode);
#endif

out_nolock:
	sb_end_write(inode->i_sb);

	ODINFS_END_TIMING(xip_write_t, xip_write_time);
	return ret;
}

/* OOM err return with xip file fault handlers doesn't mean anything.
 * It would just cause the OS to go an unnecessary killing spree !
 */
static vm_fault_t __odinfs_xip_file_fault(struct vm_area_struct *vma,
					  struct vm_fault *vmf)
{
	struct address_space *mapping = vma->vm_file->f_mapping;
	struct inode *inode = mapping->host;
	pgoff_t size;
	void *xip_mem;
	unsigned long xip_pfn;
	vm_fault_t err;

	size = (i_size_read(inode) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (vmf->pgoff >= size) {
		odinfs_dbg(
			"[%s:%d] pgoff >= size(SIGBUS). vm_start(0x%lx),"
			" vm_end(0x%lx), pgoff(0x%lx), VA(%lx), size 0x%lx\n",
			__func__, __LINE__, vma->vm_start, vma->vm_end,
			vmf->pgoff, (unsigned long)vmf->address, size);
		return VM_FAULT_SIGBUS;
	}

	err = odinfs_get_xip_mem(mapping, vmf->pgoff, 1, &xip_mem, &xip_pfn);
	if (unlikely(err)) {
		odinfs_dbg("[%s:%d] get_xip_mem failed(OOM). vm_start(0x%lx),"
			   " vm_end(0x%lx), pgoff(0x%lx), VA(%lx)\n",
			   __func__, __LINE__, vma->vm_start, vma->vm_end,
			   vmf->pgoff, (unsigned long)vmf->address);
		return VM_FAULT_SIGBUS;
	}

	odinfs_dbg_mmapv(
		"[%s:%d] vm_start(0x%lx), vm_end(0x%lx), pgoff(0x%lx), "
		"BlockSz(0x%lx), VA(0x%lx)->PA(0x%lx)\n",
		__func__, __LINE__, vma->vm_start, vma->vm_end, vmf->pgoff,
		PAGE_SIZE, (unsigned long)vmf->address,
		(unsigned long)xip_pfn << PAGE_SHIFT);

	err = vmf_insert_mixed(vma, (unsigned long)vmf->address,
			       pfn_to_pfn_t(xip_pfn));

	if (err == -ENOMEM)
		return VM_FAULT_SIGBUS;
	/*
	 * err == -EBUSY is fine, we've raced against another thread
	 * that faulted-in the same page
	 */
	if (err != -EBUSY)
		BUG_ON(err);
	return VM_FAULT_NOPAGE;
}

static vm_fault_t odinfs_xip_file_fault(struct vm_fault *vmf)
{
	vm_fault_t ret = 0;
	ODINFS_DEFINE_TIMING_VAR(fault_time);

	ODINFS_START_TIMING(mmap_fault_t, fault_time);
	rcu_read_lock();
	ret = __odinfs_xip_file_fault(vmf->vma, vmf);
	rcu_read_unlock();
	ODINFS_END_TIMING(mmap_fault_t, fault_time);
	return ret;
}

static int odinfs_find_and_alloc_blocks(struct inode *inode, sector_t iblock,
					sector_t *data_block, int create)
{
	int err = -EIO;
	u64 block;
	odinfs_transaction_t *trans;
	struct odinfs_inode *pi;

	block = odinfs_find_data_block(inode, iblock);

	if (!block) {
		struct super_block *sb = inode->i_sb;
		if (!create) {
			err = -ENODATA;
			goto err;
		}

		pi = odinfs_get_inode(sb, inode->i_ino);
		trans = odinfs_current_transaction();
		if (trans) {
			err = odinfs_alloc_blocks(trans, inode, iblock, 1,
						  true);
			if (err) {
				odinfs_dbg_verbose("[%s:%d] Alloc failed!\n",
						   __func__, __LINE__);
				goto err;
			}
		} else {
			/* 1 lentry for inode, 1 lentry for inode's b-tree */
			trans = odinfs_new_transaction(sb, MAX_INODE_LENTRIES);
			if (IS_ERR(trans)) {
				err = PTR_ERR(trans);
				goto err;
			}

			odinfs_add_logentry(sb, trans, pi, MAX_DATA_PER_LENTRY,
					    LE_DATA);
			err = odinfs_alloc_blocks(trans, inode, iblock, 1,
						  true);

			odinfs_commit_transaction(sb, trans);

			if (err) {
				odinfs_dbg_verbose("[%s:%d] Alloc failed!\n",
						   __func__, __LINE__);
				goto err;
			}
		}
		block = odinfs_find_data_block(inode, iblock);
		if (!block) {
			odinfs_dbg("[%s:%d] But alloc didn't fail!\n", __func__,
				   __LINE__);
			err = -ENODATA;
			goto err;
		}
	}
	odinfs_dbg_mmapvv("iblock 0x%llx allocated_block 0x%llx\n", iblock,
			  block);

	*data_block = block;
	err = 0;

err:
	return err;
}

static inline int __odinfs_get_block(struct inode *inode, pgoff_t pgoff,
				     int create, sector_t *result)
{
	int rc = 0;

	rc = odinfs_find_and_alloc_blocks(inode, (sector_t)pgoff, result,
					  create);
	return rc;
}

int odinfs_get_xip_mem(struct address_space *mapping, pgoff_t pgoff, int create,
		       void **kmem, unsigned long *pfn)
{
	int rc;
	sector_t block = 0;
	struct inode *inode = mapping->host;

	rc = __odinfs_get_block(inode, pgoff, create, &block);
	if (rc) {
		odinfs_dbg1(
			"[%s:%d] rc(%d), sb->physaddr(0x%llx), block(0x%llx),"
			" pgoff(0x%lx), flag(0x%x), PFN(0x%lx)\n",
			__func__, __LINE__, rc,
			ODINFS_SB(inode->i_sb)->phys_addr, block, pgoff, create,
			*pfn);
		return rc;
	}

	*kmem = odinfs_get_virt_addr_from_offset(inode->i_sb, block);
	*pfn = odinfs_get_pfn(inode->i_sb, block);

	odinfs_dbg_mmapvv("[%s:%d] sb->physaddr(0x%llx), block(0x%llx),"
			  " pgoff(0x%lx), flag(0x%x), PFN(0x%lx)\n",
			  __func__, __LINE__, ODINFS_SB(inode->i_sb)->phys_addr,
			  block, pgoff, create, *pfn);
	return 0;
}

static const struct vm_operations_struct odinfs_xip_vm_ops = {
	.fault = odinfs_xip_file_fault,
};

int odinfs_xip_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	//	BUG_ON(!file->f_mapping->a_ops->get_xip_mem);

	file_accessed(file);

	vm_flags_set(vma, VM_MIXEDMAP);

	vma->vm_ops = &odinfs_xip_vm_ops;
	odinfs_dbg_mmap4k("[%s:%d] MMAP 4KPAGE vm_start(0x%lx),"
			  " vm_end(0x%lx), vm_flags(0x%lx), "
			  "vm_page_prot(0x%lx)\n",
			  __func__, __LINE__, vma->vm_start, vma->vm_end,
			  vma->vm_flags, pgprot_val(vma->vm_page_prot));

	return 0;
}
