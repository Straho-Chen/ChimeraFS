#include <linux/pfn_t.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/parser.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/seq_file.h>
#include <linux/mount.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/magic.h>
#include <linux/exportfs.h>
#include <linux/random.h>
#include <linux/cred.h>
#include <linux/backing-dev.h>
#include <linux/list.h>
#include <linux/dax.h>
#include <uapi/linux/mount.h>
#include "slfs.h"
#include "journal.h"

int measure_timing = 0;

module_param(measure_timing, int, S_IRUGO);
MODULE_PARM_DESC(measure_timing, "Timing measurement");

static struct super_operations slfs_sops;
static const struct export_operations slfs_export_ops;
static struct kmem_cache *slfs_inode_cachep;
struct kmem_cache *slfs_blocknode_cachep;

static void init_once(void *foo)
{
	struct slfs_inode_info *vi = foo;

	inode_init_once(&vi->vfs_inode);
}

static int __init init_inodecache(void)
{
	slfs_inode_cachep = kmem_cache_create(
		"slfs_inode_cache", sizeof(struct slfs_inode_info), 0,
		(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), init_once);
	if (!slfs_inode_cachep)
		return -ENOMEM;
	return 0;
}

static void destroy_inodecache(void)
{
	/*
	 * Make sure all delayed rcu free inodes are flushed before
	 * we destroy cache.
	 */
	rcu_barrier();
	kmem_cache_destroy(slfs_inode_cachep);
}

struct inode *slfs_alloc_inode(struct super_block *sb)
{
	struct slfs_inode_info *vi;

	vi = alloc_inode_sb(sb, slfs_inode_cachep, GFP_NOFS);
	if (!vi)
		return NULL;

	return &vi->vfs_inode;
}

static void slfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);

	kmem_cache_free(slfs_inode_cachep, SLFS_I(inode));
}

void slfs_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, slfs_i_callback);
}

void slfs_put_super(struct super_block *sb)
{
	// TODO: Implement this function
	// release super block info resource, this action is similar to umount
	struct slfs_sb_info *sbi = SLFS_SB(sb);
}

int slfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	// TODO: Implement this function
	return 0;
}

int slfs_remount(struct super_block *sb, int *flags, char *data)
{
	// TODO: Implement this function
	return 0;
}

int slfs_show_options(struct seq_file *seq, struct dentry *root)
{
	// TODO: Implement this function
	return 0;
}

static inline void set_default_opts(struct slfs_sb_info *sbi)
{
	/* set_opt(sbi->s_mount_opt, PROTECT); */
	set_opt(sbi->s_mount_opt, HUGEIOREMAP);
	set_opt(sbi->s_mount_opt, ERRORS_CONT);
	// sbi->journal_size = SLFS_DEFAULT_JOURNAL_SIZE;
}

static int slfs_get_block_info(struct slfs_sb_info *sbi)
{
	if (pmem_ar_dev.elem_num == 0) {
		slfs_err("No device in pmem array!\n");
		return -EINVAL;
	}

	/*
   * TODO: Here we are making two assumptions.
   * 1. The pmem_ar_dev.bdevs[i] corresponds to the nvm dimm on socket[i]
   * 2. The mapping of pmem_ar_dev.bdevs does not change over the reboot
   *
   * Fix them when have time.
   */
	for (int i = 0; i < pmem_ar_dev.elem_num; i++) {
		struct dax_device *dax_dev;
		void *virt_addr = NULL;
		pfn_t __pfn_t;
		long size;

		struct block_device *bdev = pmem_ar_dev.bdevs[i];

		// pmfs just use a single numa node pmem device. We should use all numa node

		dax_dev = fs_dax_get_by_bdev(bdev, &sbi->s_dax_start_off, NULL,
					     NULL);
		if (!dax_dev) {
			slfs_err("Couldn't retrieve DAX device\n");
			return -EINVAL;
		}

		size = dax_direct_access(dax_dev, 0, LONG_MAX / PAGE_SIZE,
					 DAX_ACCESS, &virt_addr, &__pfn_t) *
		       PAGE_SIZE;

		if (size <= 0) {
			slfs_err("direct_access failed\n");
			return -EINVAL;
		}

		pmem_ar_dev.start_virt_addr[i] = (unsigned long)virt_addr;
		pmem_ar_dev.size_in_bytes[i] = size;

		if (i == 0 || virt_addr < sbi->virt_addr) {
			sbi->virt_addr = virt_addr;
			sbi->phys_addr = pfn_t_to_pfn(__pfn_t) << PAGE_SHIFT;
			sbi->initsize = size;
			sbi->head_socket = i;
		}

		slfs_debug(
			"dimm index: %d, name: %s, virt_addr: %lx, size: %ld\n",
			i, bdev->bd_disk->disk_name, (unsigned long)virt_addr,
			size);
	}

	// sbi->num_blocks = 0;

	for (int i = 0; i < pmem_ar_dev.elem_num; i++) {
		/*
     * Implicitly assume 4KB block size is bad, but it is all over slfs
     * code ...
     */

		unsigned long size_in_blocks = pmem_ar_dev.size_in_bytes[i] >>
					       PAGE_SHIFT;

		/*
     * We use the block number to hide the virtual address gap between
     * NVM dimms
     */

		sbi->block_info[i].start_block =
			(pmem_ar_dev.start_virt_addr[i] -
			 (unsigned long)sbi->virt_addr) >>
			PAGE_SHIFT;

		sbi->block_info[i].end_block =
			sbi->block_info[i].start_block + size_in_blocks - 1;

		// sbi->num_blocks += size_in_blocks;

		slfs_debug(
			"heap socket: %d, start_block: %lu, end_block: %lu\n",
			i, sbi->block_info[i].start_block,
			sbi->block_info[i].end_block);
	}

	/* duplicate the info in the sbi */
	sbi->device_num = pmem_ar_dev.elem_num;

	return 0;
}

static int slfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct slfs_super_block *super;
	struct slfs_inode *root_pi;
	struct slfs_sb_info *sbi = NULL;
	struct inode *root_i = NULL;
	unsigned long blocksize;
	uint32_t random = 0;
	int retval = -EINVAL;

	BUILD_BUG_ON(sizeof(struct slfs_super_block) > SLFS_SB_SIZE);
	BUILD_BUG_ON(sizeof(struct slfs_inode) > SLFS_INODE_SIZE);

	if (arch_has_pcommit()) {
		slfs_info("arch has PCOMMIT support\n");
		support_pcommit = 1;
	} else {
		slfs_info("arch does not have PCOMMIT support\n");
	}

	if (arch_has_clwb()) {
		slfs_info("arch has CLWB support\n");
		support_clwb = 1;
	} else {
		slfs_info("arch does not have CLWB support\n");
	}

	sbi = kzalloc(sizeof(struct slfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	set_default_opts(sbi);

	if (slfs_get_block_info(sbi))
		goto out;

	if (slfs_alloc_block_free_lists(sbi))
		goto out;

	get_random_bytes(&random, sizeof(uint32_t));
	atomic_set(&sbi->next_generation, random);

	/* Init with default values */
	sbi->mode = (S_IRUGO | S_IXUGO | S_IWUSR);
	sbi->uid = current_fsuid();
	sbi->gid = current_fsgid();
	set_opt(sbi->s_mount_opt, XIP);
	clear_opt(sbi->s_mount_opt, PROTECT);
	set_opt(sbi->s_mount_opt, HUGEIOREMAP);

	mutex_init(&sbi->inode_table_mutex);
	mutex_init(&sbi->s_lock);

out:

	return 0;
}

/*
 * the super block writes are all done "on the fly", so the
 * super block is never in a "dirty" state, so there's no need
 * for write_super.
 */
static struct super_operations slfs_sops = {
	.alloc_inode = slfs_alloc_inode,
	.destroy_inode = slfs_destroy_inode,
	.write_inode = slfs_write_inode,
	.dirty_inode = slfs_dirty_inode,
	.evict_inode = slfs_evict_inode,
	.put_super = slfs_put_super,
	.statfs = slfs_statfs,
	.remount_fs = slfs_remount,
	.show_options = slfs_show_options,
};

static struct dentry *slfs_mount(struct file_system_type *fs_type, int flags,
				 const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, slfs_fill_super);
}

static struct file_system_type slfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "slfs",
	.mount = slfs_mount,
	.kill_sb = kill_block_super,
};

static struct dentry *slfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
					int fh_len, int fh_type)
{
	// TODO: Implement this function
	return NULL;
}

static struct dentry *slfs_fh_to_parent(struct super_block *sb, struct fid *fid,
					int fh_len, int fh_type)
{
	// TODO: Implement this function
	return NULL;
}

static struct dentry *slfs_get_parent(struct dentry *child)
{
	// TODO: Implement this function
	return NULL;
}

static const struct export_operations slfs_export_ops = {
	.fh_to_dentry = slfs_fh_to_dentry,
	.fh_to_parent = slfs_fh_to_parent,
	.get_parent = slfs_get_parent,
};

static int __init init_slfs_fs(void)
{
	// TODO: Implement this function
	int ret = 0;

	ret = init_inodecache();
	if (ret)
		goto out1;

	return 0;

out1:
	destroy_inodecache();
	return ret;
}

static void __exit exit_slfs_fs(void)
{
	// TODO: Implement this function
	destroy_inodecache();
}

MODULE_AUTHOR("Straho Chen <strahochen@foxmail.com>");
MODULE_DESCRIPTION("Single Level File System");
MODULE_LICENSE("GPL");

module_init(init_slfs_fs) module_exit(exit_slfs_fs)