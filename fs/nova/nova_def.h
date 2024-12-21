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

#include <linux/crc32c.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <linux/types.h>

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
{ /* TODO: Fix me. */
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

/* At maximum, do this amount of memcpy and then do cond_resched() */
/* 32KB currently */
#define NOVA_MEMCPY_CHUNK_SIZE (8 * 4096)

#define NOVA_MEMCPY_NT 1

#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

/* #define nova_dbg(s, args...)		pr_debug(s, ## args) */
#define nova_dbg(s, args...) pr_info(s, ##args)
#define nova_dbg1(s, args...)
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

extern unsigned int blk_type_to_shift[NOVA_BLOCK_TYPE_MAX];
extern unsigned int blk_type_to_size[NOVA_BLOCK_TYPE_MAX];

#define MMAP_WRITE_BIT 0x20UL // mmaped for write
#define IS_MAP_WRITE(p) ((p) & (MMAP_WRITE_BIT))
#define MMAP_ADDR(p) ((p) & (PAGE_MASK))

/* Mask out flags that are inappropriate for the given type of inode. */
static inline __le32 nova_mask_flags(umode_t mode, __le32 flags)
{
	flags &= cpu_to_le32(NOVA_FL_INHERITED);
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & cpu_to_le32(NOVA_REG_FLMASK);
	else
		return flags & cpu_to_le32(NOVA_OTHER_FLMASK);
}

/* Update the crc32c value by appending a 64b data word. */
#define nova_crc32c_qword(qword, crc)                 \
	do {                                          \
		asm volatile("crc32q %1, %0"          \
			     : "=r"(crc)              \
			     : "r"(qword), "0"(crc)); \
	} while (0)

static inline u32 nova_crc32c(u32 crc, const u8 *data, size_t len)
{
	u8 *ptr = (u8 *)data;
	u64 acc = crc; /* accumulator, crc32c value in lower 32b */
	u32 csum;

	/* x86 instruction crc32 is part of SSE-4.2 */
	if (static_cpu_has(X86_FEATURE_XMM4_2)) {
		/* This inline assembly implementation should be equivalent
     * to the kernel's crc32c_intel_le_hw() function used by
     * crc32c(), but this performs better on test machines.
     */
		while (len > 8) {
			asm volatile(/* 64b quad words */
				     "crc32q (%1), %0"
				     : "=r"(acc)
				     : "r"(ptr), "0"(acc));
			ptr += 8;
			len -= 8;
		}

		while (len > 0) {
			asm volatile(/* trailing bytes */
				     "crc32b (%1), %0"
				     : "=r"(acc)
				     : "r"(ptr), "0"(acc));
			ptr++;
			len--;
		}

		csum = (u32)acc;
	} else {
		/* The kernel's crc32c() function should also detect and use the
     * crc32 instruction of SSE-4.2. But calling in to this function
     * is about 3x to 5x slower than the inline assembly version on
     * some test machines.
     */
		csum = crc32c(crc, data, len);
	}

	return csum;
}

/* uses CPU instructions to atomically write up to 8 bytes */
static inline void nova_memcpy_atomic(void *dst, const void *src, u8 size)
{
	switch (size) {
	case 1: {
		volatile u8 *daddr = dst;
		const u8 *saddr = src;
		*daddr = *saddr;
		break;
	}
	case 2: {
		volatile __le16 *daddr = dst;
		const u16 *saddr = src;
		*daddr = cpu_to_le16(*saddr);
		break;
	}
	case 4: {
		volatile __le32 *daddr = dst;
		const u32 *saddr = src;
		*daddr = cpu_to_le32(*saddr);
		break;
	}
	case 8: {
		volatile __le64 *daddr = dst;
		const u64 *saddr = src;
		*daddr = cpu_to_le64(*saddr);
		break;
	}
	default:
		nova_dbg("error: memcpy_atomic called with %d bytes\n", size);
		// BUG();
	}
}

static inline int memcpy_to_pmem_nocache(void *dst, const void *src,
					 unsigned int size)
{
	int ret;

	ret = __copy_from_user_inatomic_nocache(dst, src, size);

	return ret;
}

static inline int memcpy_to_pmem_flush(void *dst, const void *src,
				       unsigned int size)
{
	int ret;

	ret = __copy_from_user(dst, src, size);

	nova_flush_buffer(dst, size - ret, 0);

	return ret;
}

/* assumes the length to be 4-byte aligned */
static inline void memset_nt(void *dest, uint32_t dword, size_t length)
{
	uint64_t dummy1, dummy2;
	uint64_t qword = ((uint64_t)dword << 32) | dword;

	asm volatile("movl %%edx,%%ecx\n"
		     "andl $63,%%edx\n"
		     "shrl $6,%%ecx\n"
		     "jz 9f\n"
		     "1:	 movnti %%rax,(%%rdi)\n"
		     "2:	 movnti %%rax,1*8(%%rdi)\n"
		     "3:	 movnti %%rax,2*8(%%rdi)\n"
		     "4:	 movnti %%rax,3*8(%%rdi)\n"
		     "5:	 movnti %%rax,4*8(%%rdi)\n"
		     "8:	 movnti %%rax,5*8(%%rdi)\n"
		     "7:	 movnti %%rax,6*8(%%rdi)\n"
		     "8:	 movnti %%rax,7*8(%%rdi)\n"
		     "leaq 64(%%rdi),%%rdi\n"
		     "decl %%ecx\n"
		     "jnz 1b\n"
		     "9:	movl %%edx,%%ecx\n"
		     "andl $7,%%edx\n"
		     "shrl $3,%%ecx\n"
		     "jz 11f\n"
		     "10:	 movnti %%rax,(%%rdi)\n"
		     "leaq 8(%%rdi),%%rdi\n"
		     "decl %%ecx\n"
		     "jnz 10b\n"
		     "11:	 movl %%edx,%%ecx\n"
		     "shrl $2,%%ecx\n"
		     "jz 12f\n"
		     "movnti %%eax,(%%rdi)\n"
		     "12:\n"
		     : "=D"(dummy1), "=d"(dummy2)
		     : "D"(dest), "a"(qword), "d"(length)
		     : "memory", "rcx");
}

// super block
/*
 * Structure of the NOVA super block in PMEM
 *
 * The fields are partitioned into static and dynamic fields. The static fields
 * never change after file system creation. This was primarily done because
 * nova_get_block() returns NULL if the block offset is 0 (helps in catching
 * bugs). So if we modify any field using journaling (for consistency), we
 * will have to modify s_sum which is at offset 0. So journaling code fails.
 * This (static+dynamic fields) is a temporary solution and can be avoided
 * once the file system becomes stable and nova_get_block() returns correct
 * pointers even for offset 0.
 */
struct nova_super_block {
	/* static fields. they never change after file system creation.
   * checksum only validates up to s_start_dynamic field below
   */
	__le32 s_sum; /* checksum of this sb */
	__le32 s_magic; /* magic signature */
	__le32 s_padding32;
	__le32 s_blocksize; /* blocksize in bytes */
	__le64 s_size; /* total size of fs in bytes */
	char s_volume_name[16]; /* volume name */

	/* all the dynamic fields should go here */
	__le64 s_epoch_id; /* Epoch ID */

	/* s_mtime and s_wtime should be together and their order should not be
   * changed. we use an 8 byte write to update both of them atomically
   */
	__le32 s_mtime; /* mount time */
	__le32 s_wtime; /* write time */

	/* Metadata and data protections */
	u8 s_padding8;
	u8 s_metadata_csum;
	u8 s_data_csum;
	u8 s_data_parity;
} __attribute((__packed__));

#define NOVA_SB_SIZE 512 /* must be power of two */

/*
 * NOVA super-block data in DRAM
 */
struct nova_sb_info {
	struct super_block *sb; /* VFS super block */
	struct nova_super_block *nova_sb; /* DRAM copy of SB */
	struct block_device *s_bdev;
	struct dax_device *s_dax_dev;

	/*
   * base physical and virtual address of NOVA (which is also
   * the pointer to the super block)
   */
	phys_addr_t phys_addr;
	void *virt_addr;
	void *replica_reserved_inodes_addr;
	void *replica_sb_addr;

	unsigned long num_blocks;

	/* TODO: Remove this, since it's unused */
	/*
   * Backing store option:
   * 1 = no load, 2 = no store,
   * else do both
   */
	unsigned int nova_backing_option;

	/* Mount options */
	unsigned long bpi;
	unsigned long blocksize;
	unsigned long initsize;
	unsigned long s_mount_opt;
	kuid_t uid; /* Mount uid for root directory */
	kgid_t gid; /* Mount gid for root directory */
	umode_t mode; /* Mount mode for root directory */
	atomic_t next_generation;
	/* inode tracking */
	unsigned long s_inodes_used_count;
	unsigned long head_reserved_blocks;
	unsigned long tail_reserved_blocks;

	struct mutex s_lock; /* protects the SB's buffer-head */

	int cpus;
	struct proc_dir_entry *s_proc;

	/* Snapshot related */
	struct nova_inode_info *snapshot_si;
	struct radix_tree_root snapshot_info_tree;
	int num_snapshots;
	/* Current epoch. volatile guarantees visibility */
	volatile u64 s_epoch_id;
	volatile int snapshot_taking;

	int mount_snapshot;
	u64 mount_snapshot_epoch_id;

	struct task_struct *snapshot_cleaner_thread;
	wait_queue_head_t snapshot_cleaner_wait;
	wait_queue_head_t snapshot_mmap_wait;
	void *curr_clean_snapshot_info;

	/* DAX-mmap snapshot structures */
	struct mutex vma_mutex;
	struct list_head mmap_sih_list;

	/* ZEROED page for cache page initialized */
	void *zeroed_page;

	/* Checksum and parity for zero block */
	u32 zero_csum[8];
	void *zero_parity;

	/* Per-CPU journal lock */
	spinlock_t *journal_locks;

	/* Per-CPU inode map */
	struct inode_map *inode_maps;

	/* Decide new inode map id */
	unsigned long map_id;

	/* Per-CPU free block list */
	struct free_list *free_lists;
	unsigned long per_list_blocks;
};

static inline struct nova_sb_info *NOVA_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

/* ======================= Reserved blocks ========================= */

/*
 * Block 0 contains super blocks;
 * Block 1 contains reserved inodes;
 * Block 2 - 15 are reserved.
 * Block 16 - 31 contain pointers to inode table.
 * Block 32 - 47 contain pointers to replica inode table.
 * Block 48 - 63 contain pointers to journal pages.
 *
 * If data protection is enabled, more blocks are reserverd for checksums and
 * parities and the number is derived according to the whole storage size.
 */
#define HEAD_RESERVED_BLOCKS 64
#define NUM_JOURNAL_PAGES 16

#define SUPER_BLOCK_START 0 // Superblock
#define RESERVE_INODE_START 1 // Reserved inodes
#define INODE_TABLE0_START 16 // inode table
#define INODE_TABLE1_START 32 // replica inode table
#define JOURNAL_START 48 // journal pointer table

/* For replica super block and replica reserved inodes */
#define TAIL_RESERVED_BLOCKS 2

enum bm_type {
	BM_4K = 0,
	BM_2M,
	BM_1G,
};

struct single_scan_bm {
	unsigned long bitmap_size;
	unsigned long *bitmap;
};

struct scan_bitmap {
	struct single_scan_bm scan_bm_4K;
	struct single_scan_bm scan_bm_2M;
	struct single_scan_bm scan_bm_1G;
};

struct inode_map {
	struct mutex inode_table_mutex;
	struct rb_root inode_inuse_tree;
	unsigned long num_range_node_inode;
	struct nova_range_node *first_inode_range;
	int allocated;
	int freed;
};

/* ======================= Reserved inodes ========================= */

/* We have space for 31 reserved inodes */
#define NOVA_ROOT_INO (1)
#define NOVA_INODETABLE_INO \
	(2) /* Fake inode associated with inode           \
                                  * stroage.  We need this because our         \
                                  * allocator requires inode to be             \
                                  * associated with each allocation.           \
                                  * The data actually lives in linked          \
                                  * lists in INODE_TABLE0_START. */
#define NOVA_BLOCKNODE_INO (3) /* Storage for allocator state */
#define NOVA_LITEJOURNAL_INO (4) /* Storage for lightweight journals */
#define NOVA_INODELIST_INO (5) /* Storage for Inode free list */
#define NOVA_SNAPSHOT_INO (6) /* Storage for snapshot state */
#define NOVA_TEST_PERF_INO (7)

/* Normal inode starts at 32 */
#define NOVA_NORMAL_INODE_START (32)

enum nova_new_inode_type {
	TYPE_CREATE = 0,
	TYPE_MKNOD,
	TYPE_SYMLINK,
	TYPE_MKDIR
};

/*
 * Structure of an inode in PMEM
 * Keep the inode size to within 120 bytes: We use the last eight bytes
 * as inode table tail pointer.
 */
struct nova_inode {
	/* first 40 bytes */
	u8 i_rsvd; /* reserved. used to be checksum */
	u8 valid; /* Is this inode valid? */
	u8 deleted; /* Is this inode deleted? */
	u8 i_blk_type; /* data block size this inode uses */
	__le32 i_flags; /* Inode flags */
	__le64 i_size; /* Size of data in bytes */
	__le32 i_ctime; /* Inode modification time */
	__le32 i_mtime; /* Inode b-tree Modification time */
	__le32 i_atime; /* Access time */
	__le16 i_mode; /* File mode */
	__le16 i_links_count; /* Links count */

	__le64 i_xattr; /* Extended attribute block */

	/* second 40 bytes */
	__le32 i_uid; /* Owner Uid */
	__le32 i_gid; /* Group Id */
	__le32 i_generation; /* File version (for NFS) */
	__le32 i_create_time; /* Create time */
	__le64 nova_ino; /* nova inode number */

	__le64 log_head; /* Log head pointer */
	__le64 log_tail; /* Log tail pointer */

	/* last 40 bytes */
	__le64 alter_log_head; /* Alternate log head pointer */
	__le64 alter_log_tail; /* Alternate log tail pointer */

	__le64 create_epoch_id; /* Transaction ID when create */
	__le64 delete_epoch_id; /* Transaction ID when deleted */

	struct {
		__le32 rdev; /* major/minor # */
	} dev; /* device inode */

	__le32 csum; /* CRC32 checksum */

	/* Leave 8 bytes for inode table tail pointer */
} __attribute((__packed__));

/*
 * Inode table.  It's a linked list of pages.
 */
struct inode_table {
	__le64 log_head;
};

/*
 * NOVA-specific inode state kept in DRAM
 */
struct nova_inode_info_header {
	/* Map from file offsets to write log entries. */
	struct radix_tree_root tree;
	struct rb_root rb_tree; /* RB tree for directory */
	struct rb_root vma_tree; /* Write vmas */
	struct list_head list; /* SB list of mmap sih */
	int num_vmas;
	unsigned short i_mode; /* Dir or file? */
	unsigned int i_flags;
	unsigned long log_pages; /* Num of log pages */
	unsigned long i_size;
	unsigned long i_blocks;
	unsigned long ino;
	unsigned long pi_addr;
	unsigned long alter_pi_addr;
	unsigned long valid_entries; /* For thorough GC */
	unsigned long num_entries; /* For thorough GC */
	u64 last_setattr; /* Last setattr entry */
	u64 last_link_change; /* Last link change entry */
	u64 last_dentry; /* Last updated dentry */
	u64 trans_id; /* Transaction ID */
	u64 log_head; /* Log head pointer */
	u64 log_tail; /* Log tail pointer */
	u64 alter_log_head; /* Alternate log head pointer */
	u64 alter_log_tail; /* Alternate log tail pointer */
	u8 i_blk_type;
};

/* For rebuild purpose, temporarily store pi infomation */
struct nova_inode_rebuild {
	u64 i_size;
	u32 i_flags; /* Inode flags */
	u32 i_ctime; /* Inode modification time */
	u32 i_mtime; /* Inode b-tree Modification time */
	u32 i_atime; /* Access time */
	u32 i_uid; /* Owner Uid */
	u32 i_gid; /* Group Id */
	u32 i_generation; /* File version (for NFS) */
	u16 i_links_count; /* Links count */
	u16 i_mode; /* File mode */
	u64 trans_id;
};

/*
 * DRAM state for inodes
 */
struct nova_inode_info {
	struct nova_inode_info_header header;
	struct inode vfs_inode;
};

// snapshot
/*
 * DRAM log of updates to a snapshot.
 */
struct snapshot_list {
	struct mutex list_mutex;
	unsigned long num_pages;
	unsigned long head;
	unsigned long tail;
};

/*
 * DRAM info about a snapshop.
 */
struct snapshot_info {
	u64 epoch_id;
	u64 timestamp;
	unsigned long snapshot_entry; /* PMEM pointer to the struct
				       * snapshot_info_entry for this
				       * snapshot
				       */

	struct snapshot_list *lists; /* Per-CPU snapshot list */
};

enum nova_snapshot_entry_type {
	SS_INODE = 1,
	SS_FILE_WRITE,
};

/*
 * Snapshot log entry for recording an inode operation in a snapshot log.
 *
 * Todo: add checksum
 */
struct snapshot_inode_entry {
	u8 type;
	u8 deleted;
	u8 padding[6];
	u64 padding64;
	u64 nova_ino; // inode number that was deleted.
	u64 delete_epoch_id; // Deleted when?
} __attribute((__packed__));

/*
 * Snapshot log entry for recording a write operation in a snapshot log
 *
 * Todo: add checksum.
 */
struct snapshot_file_write_entry {
	u8 type;
	u8 deleted;
	u8 padding[6];
	u64 nvmm;
	u64 num_pages;
	u64 delete_epoch_id;
} __attribute((__packed__));

/*
 * PMEM structure pointing to a log comprised of snapshot_inode_entry and
 * snapshot_file_write_entry objects.
 *
 * TODO: add checksum
 */
struct snapshot_nvmm_list {
	__le64 padding;
	__le64 num_pages;
	__le64 head;
	__le64 tail;
} __attribute((__packed__));

/* Support up to 128 CPUs */
struct snapshot_nvmm_page {
	struct snapshot_nvmm_list lists[128];
};

/* ======================= Log entry ========================= */
/* Inode entry in the log */

#define MAIN_LOG 0
#define ALTER_LOG 1

#define PAGE_OFFSET_MASK 4095
#define BLOCK_OFF(p) ((p) & ~PAGE_OFFSET_MASK)

#define ENTRY_LOC(p) ((p)&PAGE_OFFSET_MASK)

#define LOG_BLOCK_TAIL 4064
#define PAGE_TAIL(p) (BLOCK_OFF(p) + LOG_BLOCK_TAIL)

/*
 * Log page state and pointers to next page and the replica page
 */
struct nova_inode_page_tail {
	__le32 invalid_entries;
	__le32 num_entries;
	__le64 epoch_id; /* For snapshot list page */
	__le64 alter_page; /* Corresponding page in the other log */
	__le64 next_page;
} __attribute((__packed__));

/* Fit in PAGE_SIZE */
struct nova_inode_log_page {
	char padding[LOG_BLOCK_TAIL];
	struct nova_inode_page_tail page_tail;
} __attribute((__packed__));

#define EXTEND_THRESHOLD 256

enum nova_entry_type {
	FILE_WRITE = 1,
	DIR_LOG,
	SET_ATTR,
	LINK_CHANGE,
	MMAP_WRITE,
	SNAPSHOT_INFO,
	NEXT_PAGE,
};

static inline u8 nova_get_entry_type(void *p)
{
	u8 type;

	if (memcpy(&type, p, sizeof(u8)) == NULL)
		return 1;

	return type;
}

static inline void nova_set_entry_type(void *p, enum nova_entry_type type)
{
	*(u8 *)p = type;
}

/*
 * Write log entry.  Records a write to a contiguous range of PMEM pages.
 *
 * Documentation/filesystems/nova.txt contains descriptions of some fields.
 */
struct nova_file_write_entry {
	u8 entry_type;
	u8 reassigned; /* Data is not latest */
	u8 updating; /* Data is being written */
	u8 padding;
	__le32 num_pages;
	__le64 block; /* offset of first block in this write */
	__le64 pgoff; /* file offset at the beginning of this write */
	__le32 invalid_pages; /* For GC */
	/* For both ctime and mtime */
	__le32 mtime;
	__le64 size; /* File size after this write */
	__le64 epoch_id;
	__le64 trans_id;
	__le32 csumpadding;
	__le32 csum;
} __attribute((__packed__));

#define WENTRY(entry) ((struct nova_file_write_entry *)entry)

/*
 * Log entry for adding a file/directory to a directory.
 *
 * Update DIR_LOG_REC_LEN if modify this struct!
 */
struct nova_dentry {
	u8 entry_type;
	u8 name_len; /* length of the dentry name */
	u8 reassigned; /* Currently deleted */
	u8 invalid; /* Invalid now? */
	__le16 de_len; /* length of this dentry */
	__le16 links_count;
	__le32 mtime; /* For both mtime and ctime */
	__le32 csum; /* entry checksum */
	__le64 ino; /* inode no pointed to by this entry */
	__le64 padding;
	__le64 epoch_id;
	__le64 trans_id;
	char name[NOVA_NAME_LEN + 1]; /* File name */
} __attribute((__packed__));

#define DENTRY(entry) ((struct nova_dentry *)entry)

#define NOVA_DIR_PAD 8 /* Align to 8 bytes boundary */
#define NOVA_DIR_ROUND (NOVA_DIR_PAD - 1)
#define NOVA_DENTRY_HEADER_LEN 48
#define NOVA_DIR_LOG_REC_LEN(name_len)                                \
	(((name_len + 1) + NOVA_DENTRY_HEADER_LEN + NOVA_DIR_ROUND) & \
	 ~NOVA_DIR_ROUND)

#define NOVA_MAX_ENTRY_LEN NOVA_DIR_LOG_REC_LEN(NOVA_NAME_LEN)

/*
 * Log entry for updating file attributes.
 */
struct nova_setattr_logentry {
	u8 entry_type;
	u8 attr; /* bitmap of which attributes to update */
	__le16 mode;
	__le32 uid;
	__le32 gid;
	__le32 atime;
	__le32 mtime;
	__le32 ctime;
	__le64 size; /* File size after truncation */
	__le64 epoch_id;
	__le64 trans_id;
	u8 invalid;
	u8 paddings[3];
	__le32 csum;
} __attribute((__packed__));

#define SENTRY(entry) ((struct nova_setattr_logentry *)entry)

/* Link change log entry.
 *
 * TODO: Do we need this to be 32 bytes?
 */
struct nova_link_change_entry {
	u8 entry_type;
	u8 invalid;
	__le16 links;
	__le32 ctime;
	__le32 flags;
	__le32 generation; /* for NFS handles */
	__le64 epoch_id;
	__le64 trans_id;
	__le32 csumpadding;
	__le32 csum;
} __attribute((__packed__));

#define LCENTRY(entry) ((struct nova_link_change_entry *)entry)

/*
 * MMap entry.  Records the fact that a region of the file is mmapped, so
 * parity and checksums are inoperative.
 */
struct nova_mmap_entry {
	u8 entry_type;
	u8 invalid;
	u8 paddings[6];
	__le64 epoch_id;
	__le64 pgoff;
	__le64 num_pages;
	__le32 csumpadding;
	__le32 csum;
} __attribute((__packed__));

#define MMENTRY(entry) ((struct nova_mmap_entry *)entry)

/*
 * Log entry for the creation of a snapshot.  Only occurs in the log of the
 * dedicated snapshot inode.
 */
struct nova_snapshot_info_entry {
	u8 type;
	u8 deleted;
	u8 paddings[6];
	__le64 epoch_id;
	__le64 timestamp;
	__le64 nvmm_page_addr;
	__le32 csumpadding;
	__le32 csum;
} __attribute((__packed__));

#define SNENTRY(entry) ((struct nova_snapshot_info_entry *)entry)

/*
 * Transient DRAM structure that describes changes needed to append a log entry
 * to an inode
 */
struct nova_inode_update {
	u64 head;
	u64 alter_head;
	u64 tail;
	u64 alter_tail;
	u64 curr_entry;
	u64 alter_entry;
	struct nova_dentry *create_dentry;
	struct nova_dentry *delete_dentry;
};

/*
 * Transient DRAM structure to parameterize the creation of a log entry.
 */
struct nova_log_entry_info {
	enum nova_entry_type type;
	struct iattr *attr;
	struct nova_inode_update *update;
	void *data; /* struct dentry */
	u64 epoch_id;
	u64 trans_id;
	u64 curr_p; /* output */
	u64 file_size; /* de_len for dentry */
	u64 ino;
	u32 time;
	int link_change;
	int inplace; /* For file write entry */
};

static inline size_t nova_get_log_entry_size(struct super_block *sb,
					     enum nova_entry_type type)
{
	size_t size = 0;

	switch (type) {
	case FILE_WRITE:
		size = sizeof(struct nova_file_write_entry);
		break;
	case DIR_LOG:
		size = NOVA_DENTRY_HEADER_LEN;
		break;
	case SET_ATTR:
		size = sizeof(struct nova_setattr_logentry);
		break;
	case LINK_CHANGE:
		size = sizeof(struct nova_link_change_entry);
		break;
	case MMAP_WRITE:
		size = sizeof(struct nova_mmap_entry);
		break;
	case SNAPSHOT_INFO:
		size = sizeof(struct nova_snapshot_info_entry);
		break;
	default:
		break;
	}

	return size;
}

#endif /* _LINUX_NOVA_DEF_H */
