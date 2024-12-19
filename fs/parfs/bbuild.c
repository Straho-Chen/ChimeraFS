#include <linux/fs.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include "slfs.h"

static void slfs_init_bitmap_from_inode(struct super_block* sb) {
}


int slfs_fast_recovery(struct super_block *sb) {
}

int slfs_setup_blocknode_map(struct super_block *sb) {

    // TODO: two path: one for the init time, one for the recovery time
    struct slfs_super_block *super = slfs_get_super(sb);
    struct slfs_inode *si = slfs_get_inode(sb, SLFS_ROOT_INO);
    struct slfs_journal *journal = slfs_get_journal(sb);
    struct slfs_sb_info *sbi = SLFS_SB(sb);
    struct slfs_bitmap bm;
    unsigned long initsize = le64_to_cpu(super->s_size);
    ktime_t start, end;

    // Always check recovery time
    if (measure_timing == 0)
        SLFS_START_TIMING(recovery_t, start);

    mutex_init(&sbi->inode_table_mutex);

    if (slfs_fast_recovery(sb)) {
        slfs_info("SLFS: normal restart\n");
        goto end;
    }

    // allocate memory for bitmap
    slfs_debug("SLFS: Start failure recovery\n");
    bm.bitmap_4k.bitmap_size = (initsize >> (PAGE_SHIFT + 0x3));
    bm.bitmap_2M.bitmap_size = (initsize >> (PMD_SHIFT + 0x3));
    bm.bitmap_1G.bitmap_size = (initsize >> (PUD_SHIFT + 0x3));
    
    bm.bitmap_4k.bitmap = kmalloc(bm.bitmap_4k.bitmap_size, GFP_KERNEL);
    bm.bitmap_2M.bitmap = kmalloc(bm.bitmap_2M.bitmap_size, GFP_KERNEL);
    bm.bitmap_1G.bitmap = kmalloc(bm.bitmap_1G.bitmap_size, GFP_KERNEL);

    if (!bm.bitmap_4k.bitmap || !bm.bitmap_2M.bitmap || !bm.bitmap_1G.bitmap)
    {
        slfs_warn("SLFS: failed to allocate memory for bitmap\n");
        goto skip;
    }

skip:
    kfree(bm.bitmap_4k.bitmap);
    kfree(bm.bitmap_2M.bitmap);
    kfree(bm.bitmap_1G.bitmap);

end:
    if (measure_timing == 0) {
        SLFS_END_TIMING(recovery_t, start);
    }

    return 0;
}