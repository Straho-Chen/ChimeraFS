#include "pmfs.h"

const char *Timingstring[TIMING_NUM] = {
	"create",	  "unlink",	  "readdir",
	"xip_read",	  "xip_write",	  "xip_write_fast",
	"internal_write", "memcpy_read",  "memcpy_write",
	"alloc_blocks",	  "index_blocks", "increase_btree_height",
	"new_trans",	  "add_logentry", "commit_trans",
	"mmap_fault",	  "fsync",	  "free_tree",
	"evict_inode",	  "recovery",
};

u64 Timingmetastats[META_TIMING_NUM];
DEFINE_PER_CPU(u64[META_TIMING_NUM], Timingmetastats_percpu);
u64 Countmetastats[META_TIMING_NUM];
DEFINE_PER_CPU(u64[META_TIMING_NUM], Countmetastats_percpu);

u64 Timingstats[TIMING_NUM];
DEFINE_PER_CPU(u64[TIMING_NUM], Timingstats_percpu);
u64 Countstats[TIMING_NUM];
DEFINE_PER_CPU(u64[TIMING_NUM], Countstats_percpu);

atomic64_t fsync_pages = ATOMIC_INIT(0);

void pmfs_print_IO_stats(void)
{
	printk("=========== PMFS I/O stats ===========\n");
	printk("Fsync %lld pages\n", atomic64_read(&fsync_pages));
}

static void pmfs_print_meta_stats(void)
{
	pmfs_info("=========== NOVA meta stats ===========\n");
	pmfs_info("write_total: %llu\n", Timingmetastats[bd_write_t]);
	pmfs_info("write_meta: %llu\n",
		  Timingmetastats[bd_write_t] - Timingmetastats[bd_memcpy_w_t]);
	pmfs_info("write_data: %llu\n", Timingmetastats[bd_memcpy_w_t]);
}

void pmfs_get_timing_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < TIMING_NUM; i++) {
		Timingstats[i] = 0;
		Countstats[i] = 0;
		for_each_possible_cpu(cpu) {
			Timingstats[i] += per_cpu(Timingstats_percpu[i], cpu);
			Countstats[i] += per_cpu(Countstats_percpu[i], cpu);
		}
	}

	for (i = 0; i < META_TIMING_NUM; i++) {
		Timingmetastats[i] = 0;
		Countmetastats[i] = 0;
		for_each_possible_cpu(cpu) {
			Timingmetastats[i] +=
				per_cpu(Timingmetastats_percpu[i], cpu);
			Countmetastats[i] +=
				per_cpu(Countmetastats_percpu[i], cpu);
		}
	}
}

void pmfs_print_timing_stats(void)
{
	int i;

	pmfs_get_timing_stats();

	printk("======== PMFS kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		if (measure_timing || Timingstats[i]) {
			printk("%s: count %llu, timing %llu, average %llu\n",
			       Timingstring[i], Countstats[i], Timingstats[i],
			       Countstats[i] ? Timingstats[i] / Countstats[i] :
					       0);
		} else {
			printk("%s: count %llu\n", Timingstring[i],
			       Countstats[i]);
		}
	}

	pmfs_print_IO_stats();
	pmfs_print_meta_stats();
}

void pmfs_clear_stats(void)
{
	int i;
	int cpu;

	printk("======== Clear PMFS kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		Countstats[i] = 0;
		Timingstats[i] = 0;
		for_each_possible_cpu(cpu) {
			per_cpu(Timingstats_percpu[i], cpu) = 0;
			per_cpu(Countstats_percpu[i], cpu) = 0;
		}
	}
	for (i = 0; i < META_TIMING_NUM; i++) {
		Countmetastats[i] = 0;
		Timingmetastats[i] = 0;
		for_each_possible_cpu(cpu) {
			per_cpu(Timingmetastats_percpu[i], cpu) = 0;
			per_cpu(Countmetastats_percpu[i], cpu) = 0;
		}
	}
}
