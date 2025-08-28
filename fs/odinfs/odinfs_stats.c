#include "odinfs_stats.h"
#include "odinfs.h"

const char *Timingstring_odinfs[TIMING_NUM] = {
	"create",
	"unlink",
	"readdir",
	"xip_read",
	"xip_write",
	"xip_write_fast",
	"internal_write",
	"memcpy_read",
	"memcpy_write",
	"alloc_blocks",
	"new_trans",
	"add_logentry",
	"commit_trans",
	"mmap_fault",
	"fsync",
	"free_tree",
	"evict_inode",
	"recovery",

	"read_get_block",
	"read_do_delegation",
	"read_prefault_block",
	"read_send_requests",
	"read_fini_delegation",

	"write_get_block",
	"write_do_delegation",
	"write_prefault_block",
	"write_send_requests",
	"write_ring_buffer_enque",
	"write_fini_delegation",

	"agent_receive_request",

	"agent_read_address_translation",
	"agent_read_memcpy",

	"agent_write_address_translation",
	"agent_write_memcpy",
	"free_inode",
	"create_commit_transaction",
	"add_nondir",
	"new_inode",
	"create_new_transaction"
};

unsigned long Timingstats_odinfs[TIMING_NUM];
DEFINE_PER_CPU(unsigned long[TIMING_NUM], Timingstats_percpu_odinfs);
unsigned long Countstats_odinfs[TIMING_NUM];
DEFINE_PER_CPU(unsigned long[TIMING_NUM], Countstats_percpu_odinfs);

unsigned long long Timingstats_meta_odinfs[META_TIMING_NUM];
DEFINE_PER_CPU(unsigned long long[META_TIMING_NUM],
	       Timingstats_meta_percpu_odinfs);
unsigned long long Countstats_meta_odinfs[META_TIMING_NUM];
DEFINE_PER_CPU(unsigned long long[META_TIMING_NUM],
	       Countstats_meta_percpu_odinfs);

atomic64_t fsync_pages = ATOMIC_INIT(0);

void odinfs_print_IO_stats(void)
{
	printk("=========== ODINFS I/O stats ===========\n");
	printk("Fsync %lld pages\n", atomic64_read(&fsync_pages));
}

void odinfs_print_meta_stats(void)
{
	odinfs_info("=========== ODINFS meta stats ===========\n");

	odinfs_info("write_total: %llu\n",
		    Timingstats_meta_odinfs[bd_xip_write_t]);
	odinfs_info("write_meta: %llu\n", Timingstats_meta_odinfs[bd_meta_t]);
	odinfs_info("write_data: %llu\n",
		    Timingstats_meta_odinfs[bd_xip_write_t] -
			    Timingstats_meta_odinfs[bd_meta_t]);

	// odinfs_info("read_total: %llu\n",
	// 	    Timingstats_meta_odinfs[bd_xip_read_t]);
	// odinfs_info("read_meta: %llu\n",
	// 	    Timingstats_meta_odinfs[bd_xip_read_t] -
	// 		    Timingstats_meta_odinfs[bd_agent_copy_r_t]);
	// odinfs_info("read_data: %llu\n",
	// 	    Timingstats_meta_odinfs[bd_agent_copy_r_t]);
}

static void odinfs_get_timing_stats(void)
{
	int i;
	int cpu;

	for (i = 0; i < TIMING_NUM; i++) {
		Timingstats_odinfs[i] = 0;
		Countstats_odinfs[i] = 0;
		for_each_possible_cpu(cpu) {
			Timingstats_odinfs[i] +=
				per_cpu(Timingstats_percpu_odinfs[i], cpu);
			Countstats_odinfs[i] +=
				per_cpu(Countstats_percpu_odinfs[i], cpu);
		}
	}

	for (i = 0; i < META_TIMING_NUM; i++) {
		Timingstats_meta_odinfs[i] = 0;
		Countstats_meta_odinfs[i] = 0;
		for_each_possible_cpu(cpu) {
			Timingstats_meta_odinfs[i] +=
				per_cpu(Timingstats_meta_percpu_odinfs[i], cpu);
			Countstats_meta_odinfs[i] +=
				per_cpu(Countstats_meta_percpu_odinfs[i], cpu);
		}
	}
}

void odinfs_print_timing_stats(void)
{
	int i;

	odinfs_get_timing_stats();

	printk("======== ODINFS kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		if (measure_timing || Timingstats_odinfs[i]) {
			printk("%s: count %lu, timing %lu, average %lu\n",
			       Timingstring_odinfs[i], Countstats_odinfs[i],
			       Timingstats_odinfs[i],
			       Countstats_odinfs[i] ?
				       Timingstats_odinfs[i] /
					       Countstats_odinfs[i] :
				       0);
		} else {
			printk("%s: count %lu\n", Timingstring_odinfs[i],
			       Countstats_odinfs[i]);
		}
	}

	odinfs_print_IO_stats();
	odinfs_print_meta_stats();
}

void odinfs_clear_stats(void)
{
	int i;
	int cpu;

	printk("======== Clear ODINFS kernel timing stats ========\n");
	for (i = 0; i < TIMING_NUM; i++) {
		Countstats_odinfs[i] = 0;
		Timingstats_odinfs[i] = 0;

		for_each_possible_cpu(cpu) {
			per_cpu(Timingstats_percpu_odinfs[i], cpu) = 0;
			per_cpu(Countstats_percpu_odinfs[i], cpu) = 0;
		}
	}

	for (i = 0; i < META_TIMING_NUM; i++) {
		Countstats_meta_odinfs[i] = 0;
		Timingstats_meta_odinfs[i] = 0;

		for_each_possible_cpu(cpu) {
			per_cpu(Timingstats_meta_percpu_odinfs[i], cpu) = 0;
			per_cpu(Countstats_meta_percpu_odinfs[i], cpu) = 0;
		}
	}
}
