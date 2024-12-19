/*
 * Refs:
 * https://lwn.net/Articles/58719/
 * https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html
 * https://prog.world/linux-kernel-5-0-we-write-simple-block-device-under-blk-mq/
 * linux-kernel/drivers/md/md.c
 */

#include <linux/blkdev.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/blkdev.h>

#include "pmem_ar.h"
#include "pmem_ar_block.h"

struct pmem_ar_dev pmem_ar_dev;

/* delete and unlock bdevs[start, end) */
static void do_pmem_ar_delete(int start, int end)
{
	int i = 0;

	for (i = start; i < end; i++) {
		blkdev_put(pmem_ar_dev.bdevs[i], &pmem_ar_dev);
		pmem_ar_dev.bdevs[i] = NULL;
	}
}

static int pmem_ar_create(unsigned long arg)
{
	struct pmem_arg_info pmem_arg_info;
	char *path = NULL;
	int i = 0, ret = 0;

	/* The array has already been created */
	if (pmem_ar_dev.elem_num != 0) {
		printk("ERROR: Pmem array already created with %d elements!\n",
		       pmem_ar_dev.elem_num);
		return -EINVAL;
	}

	path = kzalloc(PATH_MAX, GFP_KERNEL);

	if (copy_from_user(&pmem_arg_info, (void __user *)arg,
			   sizeof(struct pmem_arg_info))) {
		ret = -EFAULT;
		goto err_out;
	}

	for (i = 0; i < pmem_arg_info.num; i++) {
		if (strncpy_from_user(path, pmem_arg_info.paths[i], PATH_MAX) <
		    0) {
			ret = -EFAULT;
			goto err_out;
		}

		/* lock the device so that it cannot be read/write/mount by others */
		pmem_ar_dev.bdevs[i] =
			blkdev_get_by_path(path, FMODE_READ | FMODE_WRITE,
					   &pmem_ar_dev, &fs_holder_ops);

		if (IS_ERR(pmem_ar_dev.bdevs[i])) {
			printk("ERROR: Cannot get blk dev: %s. Abort.\n", path);
			ret = PTR_ERR(pmem_ar_dev.bdevs[i]);
			goto err_out;
		}
	}

	pmem_ar_dev.elem_num = pmem_arg_info.num;

	kfree(path);
	return 0;

err_out:
	/* Unlock the previously locked device */
	do_pmem_ar_delete(0, i);
	pmem_ar_dev.elem_num = 0;

	kfree(path);
	return ret;
}

static void pmem_ar_delete(void)
{
	do_pmem_ar_delete(0, pmem_ar_dev.elem_num);
	pmem_ar_dev.elem_num = 0;
}

static int pmem_ar_ioctl(struct block_device *bdev, fmode_t mode,
			 unsigned int cmd, unsigned long arg)
{
	if (cmd == SODINFS_PMEM_AR_CMD_CREATE) {
		return pmem_ar_create(arg);
	} else if (cmd == SODINFS_PMEM_AR_CMD_DELETE) {
		pmem_ar_delete();
		return 0;
	} else {
		printk("ERROR: Unknown command cmd: %u\n", cmd);
		return -EINVAL;
	}

	return 0;
}

void md_submit_bio(struct bio *bio)
{
	bio_io_error(bio);
}

static struct block_device_operations pmem_ar_ops = { .owner = THIS_MODULE,
						      .submit_bio =
							      md_submit_bio,
						      .ioctl = pmem_ar_ioctl };

void sodinfs_put_pmem_ar(void)
{
	int i = 0;

	for (i = 0; i < pmem_ar_dev.elem_num; i++) {
		if (pmem_ar_dev.dax_dev[i])
			fs_put_dax(pmem_ar_dev.dax_dev[i], NULL);
	}
}

int sodinfs_pmem_ar_init_block(void)
{
	int ret = 0;

	if ((ret = register_blkdev(SODINFS_MAJOR, SODINFS_DEV_NAME)) < 0) {
		return ret;
	}

	pmem_ar_dev.gd = blk_alloc_disk(SODINFS_PMEM_AR_NUM);

	if (pmem_ar_dev.gd == NULL) {
		printk("ERROR: alloc_disk failed!\n");
		goto out_error;
	}

	pmem_ar_dev.gd->major = SODINFS_MAJOR;
	pmem_ar_dev.gd->first_minor = 0;
	pmem_ar_dev.gd->fops = &pmem_ar_ops;
	pmem_ar_dev.gd->private_data = &pmem_ar_dev;
	strcpy(pmem_ar_dev.gd->disk_name, SODINFS_DEV_INSTANCE_NAME);

	ret = add_disk(pmem_ar_dev.gd);
	if (ret) {
		printk("ERROR: add disk failed!, error code: %d\n", ret);
		goto out_error;
	}

	return 0;

out_error:
	unregister_blkdev(SODINFS_MAJOR, SODINFS_DEV_NAME);
	return -ENOMEM;
}

void sodinfs_pmem_ar_exit_block(void)
{
	pmem_ar_delete();

	del_gendisk(pmem_ar_dev.gd);
	put_disk(pmem_ar_dev.gd);
	unregister_blkdev(SODINFS_MAJOR, SODINFS_DEV_NAME);
}
