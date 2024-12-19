#ifndef _SLFS_DEF_H
#define _SLFS_DEF_H

#include <linux/types.h>
#include <linux/fs.h>
#include "pmem_ar_block.h"

#define SLFS_NAME_LEN 255

// dirent on pm
struct slfs_direntry {
	__le64 ino; /* inode no pointed to by this entry */
	__le16 de_len; /* length of this directory entry */
	u8 name_len; /* Name length */
	u8 file_type; /* file type */
	char name[SLFS_NAME_LEN]; /* File name */
};

// inode on pm
struct slfs_inode {
	// 40 bytes
	__le32 i_flags; /* Inode flags */
	__le64 i_size; /* Size of data in bytes */
	__le32 i_atime; /* Access time */
	__le32 i_ctime; /* Inode change time */
	__le32 i_mtime; /* Inode modification time */
	__le32 i_dtime; /* Deletion Time */
	__le16 i_mode; /* File mode */
	__le16 i_links_count; /* Links count */
	__le64 i_blocks; /* Blocks count */

	// 16 bytes
	__le32 i_uid; /* Owner Uid */
	__le32 i_gid; /* Group Id */
	__le32 i_generation; /* File version (for NFS) */

	// 13 bytes
	struct {
		__le64 fpgtbl_i_pfn; /* page table pfn */
		__le32 fpgtbl_i_pte_num; /* number of pte entries of file page table */
		u8 fpgtbl_i_level; /* page table level */
	} i_file_page_table;
};

#define SLFS_INODE_SIZE 128 /* must be power of two */
// reserve inode 0 for bad block inode
#define SLFS_ROOT_INO 1

// journal on pm
struct slfs_journal {
	__le64 base;
	__le32 size;
	__le32 head;
	/* the next three fields must be in the same order and together.
	 * tail and gen_id must fall in the same 8-byte quadword */
	__le32 tail;
	__le16 gen_id; /* generation id of the log */
	__le16 pad;
	__le16 redo_logging;
};

#define SLFS_LABEL_MAX 16
#define SLFS_SB_SIZE 512 /* must be power of two */

// super block on pm
struct slfs_super_block {
	__le16 s_magic; /* magic signature */
	__le32 s_blocksize; /* blocksize in bytes */
	__le64 s_size; /* total size of fs in bytes */
	char s_volume_name[SLFS_LABEL_MAX]; /* volume name */
	/* points to the location of slfs_journal_t */
	__le64 s_journal_offset;
	/* points to the location of struct slfs_inode for the inode table */
	__le64 s_inode_table_offset;

	__le64 s_start_dynamic;

	/* s_mtime and s_wtime should be together and their order should not be
	 * changed. we use an 8 byte write to update both of them atomically */
	__le32 s_mtime; /* mount time */
	__le32 s_wtime; /* write time */
	/* fields for fast mount support. Always keep them together */
	__le64 s_free_blocks_count;
	__le64 s_blocks_hint;
	__le32 s_inodes_count;
	__le32 s_free_inodes_count;
	__le32 s_inodes_used_count;
	__le32 s_free_inode_hint;
	__le32 s_sum; /* checksum of this sb (use crc) */
};

#define SLFS_SB_STATIC_SIZE(ps) ((u64) & ps->s_start_dynamic - (u64)ps)

/*
 * Mount flags
 */
#define SLFS_MOUNT_PROTECT 0x000001 /* wprotect CR0.WP */
#define SLFS_MOUNT_XATTR_USER 0x000002 /* Extended user attributes */
#define SLFS_MOUNT_POSIX_ACL 0x000004 /* POSIX Access Control Lists */
#define SLFS_MOUNT_XIP 0x000008 /* Execute in place */
#define SLFS_MOUNT_ERRORS_CONT 0x000010 /* Continue on errors */
#define SLFS_MOUNT_ERRORS_RO 0x000020 /* Remount fs ro on errors */
#define SLFS_MOUNT_ERRORS_PANIC 0x000040 /* Panic on errors */
#define SLFS_MOUNT_HUGEMMAP 0x000080 /* Huge mappings with mmap */
#define SLFS_MOUNT_HUGEIOREMAP 0x000100 /* Huge mappings with ioremap */
#define SLFS_MOUNT_PROTECT_OLD 0x000200 /* wprotect PAGE RW Bit */
#define SLFS_MOUNT_FORMAT 0x000400 /* was FS formatted on mount? */
#define SLFS_MOUNT_MOUNTING 0x000800 /* FS currently being mounted */

/* ======================= Write ordering ========================= */

#define CACHELINE_SIZE (64)
#define CACHELINE_MASK (~(CACHELINE_SIZE - 1))
#define CACHELINE_ALIGN(addr) (((addr) + CACHELINE_SIZE - 1) & CACHELINE_MASK)

#define X86_FEATURE_PCOMMIT (9 * 32 + 22) /* PCOMMIT instruction */
#define X86_FEATURE_CLFLUSHOPT (9 * 32 + 23) /* CLFLUSHOPT instruction */
#define X86_FEATURE_CLWB (9 * 32 + 24) /* CLWB instruction */

static inline bool arch_has_pcommit(void)
{
	return static_cpu_has(X86_FEATURE_PCOMMIT);
}

static inline bool arch_has_clwb(void)
{
	return static_cpu_has(X86_FEATURE_CLWB);
}

extern int support_clwb;
extern int support_pcommit;

#define _mm_clflush(addr) \
	asm volatile("clflush %0" : "+m"(*(volatile char *)(addr)))
#define _mm_clflushopt(addr) \
	asm volatile(".byte 0x66; clflush %0" : "+m"(*(volatile char *)(addr)))
#define _mm_clwb(addr) \
	asm volatile(".byte 0x66; xsaveopt %0" : "+m"(*(volatile char *)(addr)))
#define _mm_pcommit() asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8")

/* Provides ordering from all previous clflush too */
static inline void PERSISTENT_MARK(void)
{
	/* TODO: Fix me. */
}

static inline void PERSISTENT_BARRIER(void)
{
	asm volatile("sfence\n" : :);
	if (support_pcommit) {
		/* Do nothing */
	}
}

static inline void slfs_flush_buffer(void *buf, uint32_t len, bool fence)
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
	 * provides implicit fence. */
	if (fence)
		PERSISTENT_BARRIER();
}

// timing
enum timing_category {
	create_t,
	unlink_t,
	readdir_t,
	xip_read_t,
	xip_write_t,
	xip_write_fast_t,
	internal_write_t,
	memcpy_r_t,
	memcpy_w_t,
	alloc_blocks_t,
	new_trans_t,
	add_log_t,
	commit_trans_t,
	mmap_fault_t,
	fsync_t,
	free_tree_t,
	evict_inode_t,
	recovery_t,
	TIMING_NUM,
};

extern const char *Timingstring[TIMING_NUM];
extern unsigned long long Timingstats[TIMING_NUM];
extern u64 Countstats[TIMING_NUM];

extern int measure_timing;

extern atomic64_t fsync_pages;

#define SLFS_START_TIMING(name, start)           \
	{                                        \
		if (measure_timing)              \
			start = ktime_get_raw(); \
	}

#define SLFS_END_TIMING(name, start)                        \
	{                                                   \
		if (measure_timing) {                       \
			ktime_t end;                        \
			end = ktime_get_raw();              \
			Timingstats[name] += (end - start); \
		}                                           \
		Countstats[name]++;                         \
	}

// debug code
#ifdef SLFS_LOG_TRACE
#define slfs_trace(fmt, ...) \
	pr_info("[TRACE][pid:%d]" fmt, task_pid_nr(current), ##__VA_ARGS__)
#define slfs_debug(fmt, ...) \
	pr_info("[DEBUG][pid:%d]" fmt, task_pid_nr(current), ##__VA_ARGS__)
#else
#define slfs_trace(fmt, ...)
#ifdef SLFS_LOG_DEBUG
#define slfs_debug(fmt, ...) \
	pr_info("[DEBUG][pid:%d]" fmt, task_pid_nr(current), ##__VA_ARGS__)
#else
#define slfs_debug(fmt, ...)
#endif
#endif

#ifdef SLFS_LOG_INFO
#define slfs_info(fmt, ...)                                     \
	pr_info("[cpu:%d, pid:%d]" fmt, raw_smp_processor_id(), \
		task_pid_nr(current), ##__VA_ARGS__);
#else
#define slfs_info(fmt, ...)
#endif

#define slfs_warn(fmt, ...)                                     \
	pr_warn("[cpu:%d, pid:%d]" fmt, raw_smp_processor_id(), \
		task_pid_nr(current), ##__VA_ARGS__)

#define slfs_err(fmt, ...)                                     \
	pr_err("[cpu:%d, pid:%d]" fmt, raw_smp_processor_id(), \
	       task_pid_nr(current), ##__VA_ARGS__)

// operation
#define clear_opt(o, opt) (o &= ~SLFS_MOUNT_##opt)
#define set_opt(o, opt) (o |= SLFS_MOUNT_##opt)
#define test_opt(sb, opt) (SLFS_SB(sb)->s_mount_opt & SLFS_MOUNT_##opt)

// In memory structure
// block related
struct slfs_blocknode {
	unsigned long block_low;
	unsigned long block_high;
};

struct slfs_free_list {
	/*
	 * We use maple tree to manage the free range of PM.
	 * Cause maple tree is lock free data structure, it is rcu safe, so we don't need to add lock for it.
	 */
	struct maple_tree block_free_tree;
	struct slfs_blocknode *first_node;
	struct slfs_blocknode *last_node;

	unsigned long block_start;
	unsigned long block_end;

	unsigned long num_free_blocks;
	unsigned long num_blocknodes;
};

struct slfs_bitmap_mata {
	unsigned long bitmap_size;
	unsigned long *bitmap;
};

struct slfs_bitmap {
	struct slfs_bitmap_mata bitmap_4k;
	struct slfs_bitmap_mata bitmap_2M;
	struct slfs_bitmap_mata bitmap_1G;
};

// inode info struct in memory
struct slfs_inode_info {
	struct inode vfs_inode;
};

struct slfs_dev_info {
	unsigned long start_block;
	unsigned long end_block;
};

// super block info struct in memory
struct slfs_sb_info {
	/*
	 * base physical and virtual address of SLFS (which is also
	 * the pointer to the super block)
	 */
	struct block_device *s_bdev;
	phys_addr_t phys_addr;
	void *virt_addr;

	// sb_info mutex
	struct mutex s_lock;

	unsigned long
		num_inodes; /* maximum inode number, calculate from block size */
	unsigned long blocksize; /* default 4KB, We may support specific size */
	unsigned long initsize; /* PM init size */
	unsigned long
		s_mount_opt; /* Store SLFS mount option (can store some flag for debug) */
	kuid_t uid; /* Mount uid for root directory */
	kgid_t gid; /* Mount gid for root directory */
	umode_t mode; /* Mount mode for root directory */
	atomic_t next_generation;
	int cpus; /* cpu number */
	int sockets; /* socket number */

	/* inode tracking */
	struct mutex inode_table_mutex;

	/* statfs need them */
	unsigned int s_inodes_count; /* total inodes count (used or free) */
	unsigned int s_free_inodes_count; /* free inodes count */
	/* fast build inode table and allocate */
	unsigned int s_inodes_used_count; /* used inodes count */
	unsigned int s_free_inode_hint;

	/* block tracking */
	unsigned long s_free_blocks_count;
	unsigned long s_blocks_hint;

	/* block allocation */
	struct slfs_free_list *free_list; /* per cpu free list */
	unsigned long
		head_reserved_blocks; /* reserved blocks for head (inculde journal) */
	int device_num;
	int head_socket;
	struct slfs_dev_info block_info[SLFS_PMEM_AR_MAX_DEVICE];

	// /* Journaling related structures */
	// uint32_t next_transaction_id;
	// uint32_t journal_size;
	// void *journal_base_addr;
	// struct mutex journal_mutex;
	// struct task_struct *log_cleaner_thread;
	// wait_queue_head_t log_cleaner_wait;
	// bool redo_log;

	uint64_t s_dax_start_off;
};

#endif