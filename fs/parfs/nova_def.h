/*
 * FILE NAME include/linux/nova_fs.h
 *
 * BRIEF DESCRIPTION
 *
 * Definitions for the NOVA filesystem.
 *
 * Copyright 2015-2016 Regents of the University of California,
 * UCSD Non-Volatile Systems Lab, Andiry Xu <jix024@cs.ucsd.edu>
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef _LINUX_NOVA_DEF_H
#define _LINUX_NOVA_DEF_H

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/magic.h>
#include <linux/wait.h>
#include <linux/random.h>

#include "nova_config.h"

#define NOVA_SUPER_MAGIC 0x4E4F5641 /* NOVA */

/*
 * The NOVA filesystem constants/structures
 */

/*
 * Mount flags
 */
#define NOVA_MOUNT_PROTECT 0x000001 /* wprotect CR0.WP */
#define NOVA_MOUNT_XATTR_USER 0x000002 /* Extended user attributes */
#define NOVA_MOUNT_POSIX_ACL 0x000004 /* POSIX Access Control Lists */
#define NOVA_MOUNT_DAX 0x000008 /* Direct Access */
#define NOVA_MOUNT_ERRORS_CONT 0x000010 /* Continue on errors */
#define NOVA_MOUNT_ERRORS_RO 0x000020 /* Remount fs ro on errors */
#define NOVA_MOUNT_ERRORS_PANIC 0x000040 /* Panic on errors */
#define NOVA_MOUNT_HUGEMMAP 0x000080 /* Huge mappings with mmap */
#define NOVA_MOUNT_HUGEIOREMAP 0x000100 /* Huge mappings with ioremap */
#define NOVA_MOUNT_FORMAT 0x000200 /* was FS formatted on mount? */
#define NOVA_MOUNT_DATA_COW 0x000400 /* Copy-on-write for data integrity */

/*
 * Maximal count of links to a file
 */
#define NOVA_LINK_MAX 32000

#define NOVA_DEF_BLOCK_SIZE_4K 4096

#define NOVA_INODE_BITS 7
#define NOVA_INODE_SIZE 128 /* must be power of two */

#define NOVA_NAME_LEN 255

#define MAX_CPUS 1024

/* NOVA supported data blocks */
#define NOVA_BLOCK_TYPE_4K 0
#define NOVA_BLOCK_TYPE_2M 1
#define NOVA_BLOCK_TYPE_1G 2
#define NOVA_BLOCK_TYPE_MAX 3

#define META_BLK_SHIFT 9

/*
 * Play with this knob to change the default block type.
 * By changing the NOVA_DEFAULT_BLOCK_TYPE to 2M or 1G,
 * we should get pretty good coverage in testing.
 */
#define NOVA_DEFAULT_BLOCK_TYPE NOVA_BLOCK_TYPE_4K

#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

extern void nova_error_mng(struct super_block *sb, const char *fmt, ...);

/* #define nova_dbg(s, args...)		pr_debug(s, ## args) */
#define nova_dbg(s, args...) pr_info(s, ##args)
#define nova_err(sb, s, args...) nova_error_mng(sb, s, ##args)
#define nova_warn(s, args...) pr_warn(s, ##args)
#define nova_info(s, args...) pr_info(s, ##args)

extern unsigned int nova_dbgmask;
#define NOVA_DBGMASK_MMAPHUGE (0x00000001)
#define NOVA_DBGMASK_MMAP4K (0x00000002)
#define NOVA_DBGMASK_MMAPVERBOSE (0x00000004)
#define NOVA_DBGMASK_MMAPVVERBOSE (0x00000008)
#define NOVA_DBGMASK_VERBOSE (0x00000010)
#define NOVA_DBGMASK_TRANSACTION (0x00000020)

#define nova_dbg_mmap4k(s, args...) \
	((nova_dbgmask & NOVA_DBGMASK_MMAP4K) ? nova_dbg(s, args) : 0)
#define nova_dbg_mmapv(s, args...) \
	((nova_dbgmask & NOVA_DBGMASK_MMAPVERBOSE) ? nova_dbg(s, args) : 0)
#define nova_dbg_mmapvv(s, args...) \
	((nova_dbgmask & NOVA_DBGMASK_MMAPVVERBOSE) ? nova_dbg(s, args) : 0)

#define nova_dbg_verbose(s, args...) \
	((nova_dbgmask & NOVA_DBGMASK_VERBOSE) ? nova_dbg(s, ##args) : 0)
#define nova_dbgv(s, args...) nova_dbg_verbose(s, ##args)
#define nova_dbg_trans(s, args...) \
	((nova_dbgmask & NOVA_DBGMASK_TRANSACTION) ? nova_dbg(s, ##args) : 0)

#define NOVA_ASSERT(x)                                                      \
	do {                                                                \
		if (!(x))                                                   \
			nova_warn("assertion failed %s:%d: %s\n", __FILE__, \
				  __LINE__, #x);                            \
	} while (0)

#define nova_set_bit __test_and_set_bit_le
#define nova_clear_bit __test_and_clear_bit_le
#define nova_find_next_zero_bit find_next_zero_bit_le

#define clear_opt(o, opt) (o &= ~NOVA_MOUNT_##opt)
#define set_opt(o, opt) (o |= NOVA_MOUNT_##opt)
#define test_opt(sb, opt) (NOVA_SB(sb)->s_mount_opt & NOVA_MOUNT_##opt)

#define NOVA_LARGE_INODE_TABLE_SIZE (0x200000)
/* NOVA size threshold for using 2M blocks for inode table */
#define NOVA_LARGE_INODE_TABLE_THREASHOLD (0x20000000)
/*
 * nova inode flags
 *
 * NOVA_EOFBLOCKS_FL	There are blocks allocated beyond eof
 */
#define NOVA_EOFBLOCKS_FL 0x20000000
/* Flags that should be inherited by new inodes from their parent. */
#define NOVA_FL_INHERITED                                                     \
	(FS_SECRM_FL | FS_UNRM_FL | FS_COMPR_FL | FS_SYNC_FL | FS_NODUMP_FL | \
	 FS_NOATIME_FL | FS_COMPRBLK_FL | FS_NOCOMP_FL | FS_JOURNAL_DATA_FL | \
	 FS_NOTAIL_FL | FS_DIRSYNC_FL)
/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define NOVA_REG_FLMASK (~(FS_DIRSYNC_FL | FS_TOPDIR_FL))
/* Flags that are appropriate for non-directories/regular files. */
#define NOVA_OTHER_FLMASK (FS_NODUMP_FL | FS_NOATIME_FL)
#define NOVA_FL_USER_VISIBLE (FS_FL_USER_VISIBLE | NOVA_EOFBLOCKS_FL)

/* IOCTLs */
#define NOVA_PRINT_TIMING 0xBCD00010
#define NOVA_CLEAR_STATS 0xBCD00011
#define NOVA_PRINT_LOG 0xBCD00013
#define NOVA_PRINT_LOG_BLOCKNODE 0xBCD00014
#define NOVA_PRINT_LOG_PAGES 0xBCD00015
#define NOVA_PRINT_FREE_LISTS 0xBCD00018

#define READDIR_END (ULONG_MAX)
#define INVALID_CPU (-1)
#define ANY_CPU (65536)
#define FREE_BATCH (16)
#define DEAD_ZONE_BLOCKS (256)

extern int measure_timing;
extern int metadata_csum;
extern int unsafe_metadata;
extern int wprotect;
extern int data_csum;
extern int data_parity;
extern int dram_struct_csum;

/* wait queue for delegation threads */
extern wait_queue_head_t delegation_queue[NOVA_MAX_SOCKET]
					 [NOVA_MAX_AGENT_PER_SOCKET];

extern unsigned int blk_type_to_shift[NOVA_BLOCK_TYPE_MAX];
extern unsigned int blk_type_to_size[NOVA_BLOCK_TYPE_MAX];

#define MMAP_WRITE_BIT 0x20UL // mmaped for write
#define IS_MAP_WRITE(p) ((p) & (MMAP_WRITE_BIT))
#define MMAP_ADDR(p) ((p) & (PAGE_MASK))
#define ROUNDUP_PAGE(p) (((p)&PAGE_MASK) + PAGE_SIZE)

/* ======================= Write ordering ========================= */

#define CACHELINE_SIZE (64)
#define CACHELINE_MASK (~(CACHELINE_SIZE - 1))
#define CACHELINE_ALIGN(addr) (((addr) + CACHELINE_SIZE - 1) & CACHELINE_MASK)

static inline bool arch_has_clwb(void)
{
	return static_cpu_has(X86_FEATURE_CLWB);
}

extern int support_clwb;

#define _mm_clflush(addr) \
	asm volatile("clflush %0" : "+m"(*(volatile char *)(addr)))
#define _mm_clflushopt(addr) \
	asm volatile(".byte 0x66; clflush %0" : "+m"(*(volatile char *)(addr)))
#define _mm_clwb(addr) \
	asm volatile(".byte 0x66; xsaveopt %0" : "+m"(*(volatile char *)(addr)))

/* Provides ordering from all previous clflush too */
static inline void PERSISTENT_MARK(void)
{
	/* TODO: Fix me. */
}

static inline void PERSISTENT_BARRIER(void)
{
	asm volatile("sfence\n" : :);
}

static inline void nova_flush_buffer(void *buf, uint32_t len, bool fence)
{
	uint32_t i;

	len = len + ((unsigned long)(buf) & (CACHELINE_SIZE - 1));
	if (support_clwb) {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clwb(buf + i);
	} else {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clflush(buf + i);
	}
	/* Do a fence only if asked. We often don't need to do a fence
	 * immediately after clflush because even if we get context switched
	 * between clflush and subsequent fence, the context switch operation
	 * provides implicit fence.
	 */
	if (fence)
		PERSISTENT_BARRIER();
}

/* =============== Integrity and Recovery Parameters =============== */
#define NOVA_META_CSUM_LEN (4)
#define NOVA_DATA_CSUM_LEN (4)

/* This is to set the initial value of checksum state register.
 * For CRC32C this should not matter and can be set to any value.
 */
#define NOVA_INIT_CSUM (1)

#define ADDR_ALIGN(p, bytes) ((void *)(((unsigned long)p) & ~(bytes - 1)))

/* Data stripe size in bytes and shift.
 * In NOVA this size determines the size of a checksummed stripe, and it
 * equals to the affordable lost size of data per block (page).
 * Its value should be no less than the poison radius size of media errors.
 *
 * Support NOVA_STRIPE_SHIFT <= PAGE_SHIFT (NOVA file block size shift).
 */
#define POISON_RADIUS (512)
#define POISON_MASK (~(POISON_RADIUS - 1))
#define NOVA_STRIPE_SHIFT (9) /* size should be no less than PR_SIZE */
#define NOVA_STRIPE_SIZE (1 << NOVA_STRIPE_SHIFT)

DECLARE_PER_CPU(u32, seed);

static inline u32 xor_random(void)
{
	u32 v;

	v = this_cpu_read(seed);

	if (v == 0)
		get_random_bytes(&v, sizeof(u32));

	v ^= v << 6;
	v ^= v >> 21;
	v ^= v << 7;
	this_cpu_write(seed, v);

	return v;
}

#endif /* _LINUX_NOVA_DEF_H */
