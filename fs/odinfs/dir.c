/*
 * BRIEF DESCRIPTION
 *
 * File operations for directories.
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

#include <linux/fs.h>
#include <linux/pagemap.h>

#include "balloc.h"
#include "dir.h"
#include "odinfs.h"
#include "odinfs_stats.h"

/*
 *	Parent is locked.
 */

#define DT2IF(dt) (((dt) << 12) & S_IFMT)
#define IF2DT(sif) (((sif)&S_IFMT) >> 12)

struct odinfs_direntry *odinfs_find_dentry(struct super_block *sb,
					   struct odinfs_inode *pi,
					   struct inode *inode,
					   const char *name,
					   unsigned long name_len)
{
	struct odinfs_inode_info *si = ODINFS_I(inode);
	struct odinfs_inode_info_header *sih = &si->header;
	struct odinfs_direntry *direntry = NULL;
	struct odinfs_range_node *ret_node = NULL;
	unsigned long hash;
	int found = 0;

	hash = BKDRHash(name, name_len);

	found = odinfs_find_range_node(&sih->rb_tree, hash, NODE_DIR,
				       &ret_node);
	if (found == 1 && hash == ret_node->hash)
		direntry = ret_node->direntry;

	return direntry;
}

int odinfs_insert_dir_tree(struct super_block *sb,
			   struct odinfs_inode_info_header *sih,
			   const char *name, int namelen,
			   struct odinfs_direntry *direntry)
{
	struct odinfs_range_node *node = NULL;
	unsigned long hash;
	int ret;

	hash = BKDRHash(name, namelen);

	/* FIXME: hash collision ignored here */
	node = odinfs_alloc_dir_node(sb);
	if (!node)
		return -ENOMEM;

	node->hash = hash;
	node->direntry = direntry;
	ret = odinfs_insert_range_node(&sih->rb_tree, node, NODE_DIR);
	if (ret) {
		odinfs_free_dir_node(node);
		odinfs_dbg("%s ERROR %d: %s\n", __func__, ret, name);
	}

	return ret;
}

static int odinfs_check_dentry_match(struct super_block *sb,
				     struct odinfs_direntry *dentry,
				     const char *name, int namelen)
{
	if (dentry->name_len != namelen)
		return -EINVAL;

	return strncmp(dentry->name, name, namelen);
}

int odinfs_remove_dir_tree(struct super_block *sb,
			   struct odinfs_inode_info_header *sih,
			   const char *name, int namelen,
			   struct odinfs_direntry **create_dentry)
{
	struct odinfs_direntry *entry;
	struct odinfs_range_node *ret_node = NULL;
	unsigned long hash;
	int found = 0;

	hash = BKDRHash(name, namelen);
	found = odinfs_find_range_node(&sih->rb_tree, hash, NODE_DIR,
				       &ret_node);
	if (found == 0) {
		odinfs_dbg("%s target not found: %s, length %d, "
			   "hash %lu\n",
			   __func__, name, namelen, hash);
		return -EINVAL;
	}

	entry = ret_node->direntry;
	rb_erase(&ret_node->node, &sih->rb_tree);
	odinfs_free_dir_node(ret_node);

	if (!entry) {
		odinfs_dbg("%s ERROR: %s, length %d, hash %lu\n", __func__,
			   name, namelen, hash);
		return -EINVAL;
	}

	if (entry->ino == 0 ||
	    odinfs_check_dentry_match(sb, entry, name, namelen)) {
		odinfs_dbg("%s dentry not match: %s, length %d, hash %lu\n",
			   __func__, name, namelen, hash);
		/* for debug information, still allow access to nvmm */
		odinfs_dbg(
			"dentry: inode %llu, name %s, namelen %u, rec len %u\n",
			le64_to_cpu(entry->ino), entry->name, entry->name_len,
			le16_to_cpu(entry->de_len));
		return -EINVAL;
	}

	if (create_dentry)
		*create_dentry = entry;

	return 0;
}

static int odinfs_add_dirent_to_buf(odinfs_transaction_t *trans,
				    struct dentry *dentry, struct inode *inode,
				    struct odinfs_direntry *de, u8 *blk_base,
				    struct odinfs_inode *pidir)
{
	struct inode *dir = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned short reclen;
	int nlen, rlen;
	char *top;
	struct odinfs_inode_info *si = ODINFS_I(dir);
	struct odinfs_inode_info_header *sih = &si->header;

	/*
	 * This portion sweeps through all the directory
	 * entries to find a free slot to insert this new
	 * directory entry. Needs to be optimized
	 */
	reclen = ODINFS_DIR_REC_LEN(namelen);

	if (!de) {
		top = blk_base + dir->i_sb->s_blocksize - reclen;

		if (sih->last_dentry == NULL) {
			return -ENOSPC;
		}

		de = (struct odinfs_direntry *)(sih->last_dentry);
		rlen = le16_to_cpu(de->de_len);
		if (de->ino) {
			nlen = ODINFS_DIR_REC_LEN(de->name_len);
			if (rlen - nlen >= reclen) {
				goto found_free_slot;
			} else {
				return -ENOSPC;
			}
		}

		if ((char *)de > top)
			return -ENOSPC;

		odinfs_memunlock_block(dir->i_sb, blk_base);
		de->de_len = blk_base + dir->i_sb->s_blocksize - (u8 *)de;
		odinfs_memlock_block(dir->i_sb, blk_base);
	}

found_free_slot:
	if (de->ino) {
		struct odinfs_direntry *de1;
		odinfs_add_logentry(dir->i_sb, trans, &de->de_len,
				    sizeof(de->de_len), LE_DATA);
		nlen = ODINFS_DIR_REC_LEN(de->name_len);
		de1 = (struct odinfs_direntry *)((char *)de + nlen);
		odinfs_memunlock_block(dir->i_sb, blk_base);
		de1->de_len = blk_base + dir->i_sb->s_blocksize - (u8 *)de1;
		de->de_len = cpu_to_le16(nlen);
		odinfs_memlock_block(dir->i_sb, blk_base);
		de = de1;
	} else {
		odinfs_add_logentry(dir->i_sb, trans, &de->ino, sizeof(de->ino),
				    LE_DATA);
	}

	odinfs_memunlock_block(dir->i_sb, blk_base);
	if (inode) {
		de->ino = cpu_to_le64(inode->i_ino);
	} else {
		de->ino = 0;
	}

	sih->last_dentry = (struct odinfs_direntry *)(de);

	de->name_len = namelen;
	memcpy(de->name, name, namelen);
	odinfs_memlock_block(dir->i_sb, blk_base);
	odinfs_flush_buffer(de, reclen, false);
	/*
	 * XXX shouldn't update any times until successful
	 * completion of syscall, but too many callers depend
	 * on this.
	 */
	dir->i_mtime = dir->__i_ctime = current_time(dir);
	/*dir->i_version++; */

	odinfs_memunlock_inode(dir->i_sb, pidir);
	pidir->i_mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	pidir->i_ctime = cpu_to_le32(dir->__i_ctime.tv_sec);
	odinfs_memlock_inode(dir->i_sb, pidir);

	odinfs_insert_dir_tree(dir->i_sb, sih, name, namelen, de);
	return 0;
}

/* adds a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int odinfs_add_entry(odinfs_transaction_t *trans, struct dentry *dentry,
		     struct inode *inode)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	int retval = -EINVAL;
	unsigned long block, blocks;
	struct odinfs_direntry *de;
	char *blk_base;
	struct odinfs_inode *pidir;
	u64 bp = 0;

	if (!dentry->d_name.len)
		return -EINVAL;

	pidir = odinfs_get_inode(sb, dir->i_ino);
	odinfs_add_logentry(sb, trans, pidir, MAX_DATA_PER_LENTRY, LE_DATA);

	blocks = dir->i_size >> sb->s_blocksize_bits;
	block = blocks - 1;

	if (block >= 0) {
		bp = odinfs_find_data_block(dir, block);
		blk_base = odinfs_get_virt_addr_from_offset(sb, bp);
		if (!blk_base) {
			retval = -EIO;
			goto out;
		}
		retval = odinfs_add_dirent_to_buf(trans, dentry, inode, NULL,
						  blk_base, pidir);
		if (retval != -ENOSPC)
			goto out;
	}

	retval = odinfs_alloc_blocks(trans, dir, blocks, 1, 0);
	if (retval)
		goto out;

	dir->i_size += dir->i_sb->s_blocksize;
	odinfs_update_isize(dir, pidir);

	bp = odinfs_find_data_block(dir, blocks);
	blk_base = odinfs_get_virt_addr_from_offset(sb, bp);
	if (!blk_base) {
		retval = -ENOSPC;
		goto out;
	}
	/* No need to log the changes to this de because its a new block */
	de = (struct odinfs_direntry *)blk_base;
	odinfs_memunlock_block(sb, blk_base);
	de->ino = 0;
	//de->de_len = ODINFS_DIR_REC_LEN(dentry->d_name.len);
	de->de_len = dir->i_sb->s_blocksize;
	odinfs_memlock_block(sb, blk_base);
	/* Since this is a new block, no need to log changes to this block */
	retval = odinfs_add_dirent_to_buf(NULL, dentry, inode, de, blk_base,
					  pidir);
out:
	return retval;
}

/* removes a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int odinfs_remove_entry(odinfs_transaction_t *trans, struct dentry *de,
			struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct inode *dir = de->d_parent->d_inode;
	struct odinfs_inode *pidir;
	struct qstr *entry = &de->d_name;
	struct odinfs_direntry *res_entry;
	int retval = -EINVAL;
	char *blk_base = NULL;
	struct odinfs_inode_info *si = ODINFS_I(dir);
	struct odinfs_inode_info_header *sih = &si->header;

	if (!de->d_name.len)
		return -EINVAL;

	retval = odinfs_remove_dir_tree(sb, sih, entry->name, entry->len,
					&res_entry);

	odinfs_add_logentry(sb, trans, &res_entry->ino, sizeof(res_entry->ino),
			    LE_DATA);
	odinfs_memunlock_block(sb, blk_base);
	res_entry->ino = 0;
	odinfs_memlock_block(sb, blk_base);

	dir->__i_ctime = dir->i_mtime = current_time(dir);

	pidir = odinfs_get_inode(sb, dir->i_ino);
	odinfs_add_logentry(sb, trans, pidir, MAX_DATA_PER_LENTRY, LE_DATA);

	odinfs_memunlock_inode(sb, pidir);
	pidir->i_mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	pidir->i_ctime = cpu_to_le32(dir->__i_ctime.tv_sec);
	odinfs_memlock_inode(sb, pidir);
	retval = 0;

	return retval;
}

static int odinfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct odinfs_inode *pi;
	char *blk_base;
	unsigned long offset;
	struct odinfs_direntry *de;
	ino_t ino;

	ODINFS_DEFINE_TIMING_VAR(readdir_time);

	ODINFS_START_TIMING(readdir_t, readdir_time);

	offset = ctx->pos & (sb->s_blocksize - 1);
	while (ctx->pos < inode->i_size) {
		unsigned long blk = ctx->pos >> sb->s_blocksize_bits;
		u64 bp = 0;
		bp = odinfs_find_data_block(inode, blk);
		blk_base = odinfs_get_virt_addr_from_offset(sb, bp);
		if (!blk_base) {
			odinfs_dbg(
				"directory %lu contains a hole at offset %lld\n",
				inode->i_ino, ctx->pos);
			ctx->pos += sb->s_blocksize - offset;
			continue;
		}

		while (ctx->pos < inode->i_size && offset < sb->s_blocksize) {
			de = (struct odinfs_direntry *)(blk_base + offset);

			if (!odinfs_check_dir_entry("odinfs_readdir", inode, de,
						    blk_base, offset)) {
				/* On error, skip to the next block. */
				ctx->pos = ALIGN(ctx->pos, sb->s_blocksize);
				break;
			}
			offset += le16_to_cpu(de->de_len);
			if (de->ino) {
				ino = le64_to_cpu(de->ino);
				pi = odinfs_get_inode(sb, ino);
				if (!dir_emit(ctx, de->name, de->name_len, ino,
					      IF2DT(le16_to_cpu(pi->i_mode))))
					return 0;
			}
			ctx->pos += le16_to_cpu(de->de_len);
		}
		offset = 0;
	}
	ODINFS_END_TIMING(readdir_t, readdir_time);
	return 0;
}

const struct file_operations odinfs_dir_operations = {
	.read = generic_read_dir,
	.iterate_shared = odinfs_readdir,
	.fsync = noop_fsync,
	.unlocked_ioctl = odinfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = odinfs_compat_ioctl,
#endif
};
