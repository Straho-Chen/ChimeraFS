/*
 * odinfs_stats.h
 *
 *  Created on: Sep 13, 2021
 *      Author: dzhou
 */

#ifndef __ODINFS_STATS_H_
#define __ODINFS_STATS_H_

#include <asm/msr.h>
#include <linux/cpu.h>

#include "odinfs_config.h"

extern atomic64_t fsync_pages;

static inline unsigned long odinfs_timing_start(void)
{
	unsigned long rax, rdx;
	__asm__ __volatile__("rdtscp\n" : "=a"(rax), "=d"(rdx) : : "%ecx");
	return (rdx << 32) + rax;
}

static inline unsigned long odinfs_timing_end(void)
{
	unsigned long rax, rdx;
	__asm__ __volatile__("rdtscp\n" : "=a"(rax), "=d"(rdx) : : "%ecx");
	return (rdx << 32) + rax;
}

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

	get_block_r_t,
	do_delegation_r_t,
	pre_fault_r_t,
	send_request_r_t,
	fini_delegation_r_t,

	get_block_w_t,
	do_delegation_w_t,
	pre_fault_w_t,
	send_request_w_t,
	ring_buffer_enque_w_t,
	fini_delegation_w_t,

	agent_receive_request_t,

	agent_addr_trans_r_t,
	agent_memcpy_r_t,

	agent_addr_trans_w_t,
	agent_memcpy_w_t,

	free_inode_t,
	create_commit_trans_t,
	add_nondir_t,
	new_inode_t,
	create_new_trans_t,

	TIMING_NUM,
};

enum timing_category_meta {
	bd_xip_write_t,
	bd_agent_copy_w_t,

	bd_xip_read_t,
	bd_agent_copy_r_t,

	META_TIMING_NUM,
};

extern unsigned long long Timingstats_meta_odinfs[META_TIMING_NUM];
DECLARE_PER_CPU(unsigned long long[META_TIMING_NUM],
		Timingstats_meta_percpu_odinfs);

extern unsigned long long Countstats_meta_odinfs[META_TIMING_NUM];
DECLARE_PER_CPU(unsigned long long[META_TIMING_NUM],
		Countstats_meta_percpu_odinfs);

extern int measure_meta_timing;

extern const char *Timingstring_odinfs[TIMING_NUM];

extern unsigned long Timingstats_odinfs[TIMING_NUM];
DECLARE_PER_CPU(unsigned long[TIMING_NUM], Timingstats_percpu_odinfs);

extern unsigned long Countstats_odinfs[TIMING_NUM];
DECLARE_PER_CPU(unsigned long[TIMING_NUM], Countstats_percpu_odinfs);

extern int measure_timing;

// typedef unsigned long timing_t;

#if ODINFS_MEASURE_TIME

#define increase_fsync_pages_count()        \
	{                                   \
		atomic64_inc(&fsync_pages); \
	}

// #define ODINFS_DEFINE_TIMING_VAR(name) timing_t name

typedef struct timespec64 timing_t;

#define ODINFS_DEFINE_TIMING_VAR(X) timing_t X = { 0 }

static inline void mem_fence(void)
{
	asm volatile("mfence\n" : :);
}

#define ODINFS_START_TIMING(name, start)        \
	do {                                    \
		if (measure_timing) {           \
			mem_fence();            \
			ktime_get_ts64(&start); \
			mem_fence();            \
		}                               \
	} while (0)

#define ODINFS_END_TIMING(name, start)                                         \
	do {                                                                   \
		if (measure_timing) {                                          \
			ODINFS_DEFINE_TIMING_VAR(end);                         \
			mem_fence();                                           \
			ktime_get_ts64(&end);                                  \
			mem_fence();                                           \
			__this_cpu_add(Timingstats_percpu_odinfs[name],        \
				       (end.tv_sec - start.tv_sec) *           \
						       1000000000 +            \
					       (end.tv_nsec - start.tv_nsec)); \
		}                                                              \
		__this_cpu_add(Countstats_percpu_odinfs[name], 1);             \
	} while (0)

#define ODINFS_START_META_TIMING(name, start)   \
	do {                                    \
		if (measure_meta_timing) {           \
			mem_fence();            \
			ktime_get_ts64(&start); \
			mem_fence();            \
		}                               \
	} while (0)

#define ODINFS_END_META_TIMING(name, start)                                    \
	do {                                                                   \
		if (measure_meta_timing) {                                          \
			ODINFS_DEFINE_TIMING_VAR(end);                         \
			mem_fence();                                           \
			ktime_get_ts64(&end);                                  \
			mem_fence();                                           \
			__this_cpu_add(Timingstats_meta_percpu_odinfs[name],   \
				       (end.tv_sec - start.tv_sec) *           \
						       1000000000 +            \
					       (end.tv_nsec - start.tv_nsec)); \
		}                                                              \
		__this_cpu_add(Countstats_meta_percpu_odinfs[name], 1);        \
	} while (0)
#else

#define increase_fsync_pages_count() \
	{                            \
	}

#define ODINFS_DEFINE_TIMING_VAR(name)

#define ODINFS_START_TIMING(name, start)

#define ODINFS_END_TIMING(name, start)
#endif

#endif /* __ODINFS_STATS_H_ */
