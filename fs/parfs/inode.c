#include "slfs.h"
#include <linux/pagemap.h>

int slfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	// We don't need to do anything here. All inode are clean in SLFS.
	return 0;
}

/* This function checks if VFS's inode and PMFS's inode are not in sync */
static bool slfs_is_inode_dirty(struct inode *inode, struct slfs_inode *pi)
{
	if (inode->__i_ctime.tv_sec != le32_to_cpu(pi->i_ctime) ||
	    inode->i_mtime.tv_sec != le32_to_cpu(pi->i_mtime) ||
	    inode->i_size != le64_to_cpu(pi->i_size) ||
	    inode->i_mode != le16_to_cpu(pi->i_mode) ||
	    i_uid_read(inode) != le32_to_cpu(pi->i_uid) ||
	    i_gid_read(inode) != le32_to_cpu(pi->i_gid) ||
	    inode->i_nlink != le16_to_cpu(pi->i_links_count) ||
	    inode->i_blocks != le64_to_cpu(pi->i_blocks) ||
	    inode->i_atime.tv_sec != le32_to_cpu(pi->i_atime))
		return true;
	return false;
}

/*
 * dirty_inode() is called from __mark_inode_dirty()
 *
 * usually dirty_inode should not be called because SLFS always keeps its inodes clean. 
 * Only exception is touch_atime which calls dirty_inode to update the i_atime field.
 */
void slfs_dirty_inode(struct inode *inode, int flags)
{
	// get inode
	struct super_block *sb = inode->i_sb;
	struct slfs_sb_info *sbi = SLFS_SB(sb);
	struct slfs_inode *si = slfs_get_inode(sbi, inode->i_ino);
	// flush inode, avoid read-modify-write

	/* only i_atime should have changed if at all.
	 * we can do in-place atomic update */
	slfs_memunlock_inode(sbi, si);
	si->i_atime = cpu_to_le32(inode->i_atime.tv_sec);
	slfs_memlock_inode(sbi, si);
	slfs_flush_buffer(&si->i_atime, sizeof(si->i_atime), true);

	// check if inode is dirty
	if (slfs_is_inode_dirty(inode, si)) {
		// this should not happen
		slfs_warn("inode is still dirty\n");
	}
}

/*
 * NOTE! When we get the inode, we're the only people
 * that have access to it, and as such there are no
 * race conditions we have to worry about. The inode
 * is not on the hash-lists, and it cannot be reached
 * through the filesystem because the directory entry
 * has been deleted earlier.
 * We need to free inode in disk!
 */
static int slfs_free_inode(struct inode *inode)
{
	struct slfs_sb_info *sbi = SLFS_SB(inode->i_sb);
	struct slfs_super_block *sb = slfs_get_super(sbi);
	struct slfs_inode *si = slfs_get_inode(sbi, inode->i_ino);
	int err = 0;

	// TODO: we need the transaction to prevent some data block or inode loss

	// start transaction

	// get file page table to free data pages

	// free inode

	// set super block inode related info

	// end transaction

	return err;
}

void slfs_evict_inode(struct inode *inode)
{
	// get inode
	struct slfs_sb_info *sbi = SLFS_SB(inode->i_sb);
	struct slfs_inode *si = slfs_get_inode(sbi, inode->i_ino);
	int err = 0;

	// check if the inode is bad
	if (!inode->i_nlink && !is_bad_inode(inode)) {
		// inode is bad and nobody is using it, need to delete and free
		if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
		      S_ISLNK(inode->i_mode)))
			goto out;
		if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
			goto out;

		// free inode resource
		err = slfs_free_inode(inode);
		if (err)
			goto out;
		// release slfs_inode
		si = NULL;
		inode->i_mtime = inode->__i_ctime = current_time(inode);
		inode->i_size = 0;
	}

out:
	/* TODO: Since we don't use page-cache, do we really need the following
	 * call? */
	truncate_inode_pages(&inode->i_data, 0);

	clear_inode(inode);
}
