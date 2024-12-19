#ifndef _MPROTECT_H
#define _MPROTECT_H

#include <linux/crc16.h>
#include "slfs_def.h"

/* slfs_memunlock_super() before calling! */
static inline void slfs_sync_super(struct slfs_super_block *ps)
{
	u16 crc = 0;

	ps->s_wtime = cpu_to_le32(ktime_get_seconds());
	ps->s_sum = 0;
	crc = crc16(~0, (__u8 *)ps + sizeof(__le16),
		    SLFS_SB_STATIC_SIZE(ps) - sizeof(__le16));
	ps->s_sum = cpu_to_le16(crc);
	/* Keep sync redundant super block */
	memcpy((void *)ps + SLFS_SB_SIZE, (void *)ps,
	       sizeof(struct slfs_super_block));
}

#if 0
/* slfs_memunlock_inode() before calling! */
static inline void slfs_sync_inode(struct slfs_inode *pi)
{
	u16 crc = 0;

	pi->i_sum = 0;
	crc = crc16(~0, (__u8 *)pi + sizeof(__le16), SLFS_INODE_SIZE -
		    sizeof(__le16));
	pi->i_sum = cpu_to_le16(crc);
}
#endif

static inline void wprotect_disable(void)
{
	unsigned long cr0_val;

	cr0_val = read_cr0();
	cr0_val &= (~X86_CR0_WP);
	write_cr0(cr0_val);
}

static inline void wprotect_enable(void)
{
	unsigned long cr0_val;

	cr0_val = read_cr0();
	cr0_val |= X86_CR0_WP;
	write_cr0(cr0_val);
}

/* FIXME: Assumes that we are always called in the right order.
 * slfs_writeable(vaddr, size, 1);
 * slfs_writeable(vaddr, size, 0);
 */
int slfs_writeable(void *vaddr, unsigned long size, int rw)
{
	static unsigned long flags;
	if (rw) {
		local_irq_save(flags);
		wprotect_disable();
	} else {
		wprotect_enable();
		local_irq_restore(flags);
	}
	return 0;
}

static inline int slfs_is_protected(struct slfs_sb_info *sbi)
{
	return sbi->s_mount_opt & SLFS_MOUNT_PROTECT;
}

static inline int slfs_is_wprotected(struct slfs_sb_info *sbi)
{
	return slfs_is_protected(sbi);
}

static inline void __slfs_memunlock_range(void *p, unsigned long len)
{
	/*
	 * NOTE: Ideally we should lock all the kernel to be memory safe
	 * and avoid to write in the protected memory,
	 * obviously it's not possible, so we only serialize
	 * the operations at fs level. We can't disable the interrupts
	 * because we could have a deadlock in this path.
	 */
	slfs_writeable(p, len, 1);
}

static inline void __slfs_memlock_range(void *p, unsigned long len)
{
	slfs_writeable(p, len, 0);
}

static inline void slfs_memunlock_range(struct slfs_sb_info *sbi, void *p,
					unsigned long len)
{
	if (slfs_is_protected(sbi))
		__slfs_memunlock_range(p, len);
}

static inline void slfs_memlock_range(struct slfs_sb_info *sbi, void *p,
				      unsigned long len)
{
	if (slfs_is_protected(sbi))
		__slfs_memlock_range(p, len);
}

static inline void slfs_memunlock_super(struct slfs_sb_info *sbi,
					struct slfs_super_block *ps)
{
	if (slfs_is_protected(sbi))
		__slfs_memunlock_range(ps, SLFS_SB_SIZE);
}

static inline void slfs_memlock_super(struct slfs_sb_info *sbi,
				      struct slfs_super_block *ps)
{
	slfs_sync_super(ps);
	if (slfs_is_protected(sbi))
		__slfs_memlock_range(ps, SLFS_SB_SIZE);
}

static inline void slfs_memunlock_inode(struct slfs_sb_info *sbi,
					struct slfs_inode *pi)
{
	if (slfs_is_protected(sbi))
		__slfs_memunlock_range(pi, SLFS_SB_SIZE);
}

static inline void slfs_memlock_inode(struct slfs_sb_info *sbi,
				      struct slfs_inode *pi)
{
	/* slfs_sync_inode(pi); */
	if (slfs_is_protected(sbi))
		__slfs_memlock_range(pi, SLFS_SB_SIZE);
}

static inline void slfs_memunlock_block(struct slfs_sb_info *sbi, void *bp)
{
	if (slfs_is_protected(sbi))
		__slfs_memunlock_range(bp, sbi->blocksize);
}

static inline void slfs_memlock_block(struct slfs_sb_info *sbi, void *bp)
{
	if (slfs_is_protected(sbi))
		__slfs_memlock_range(bp, sbi->blocksize);
}

#endif