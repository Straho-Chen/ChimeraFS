#include "io_stats.h"

// clang-format off
const char *IO_Timingstring[IO_TIMING_NUM] = {
    "=================== IO ===================", 
    "get_io_u", 
    "insert_working_tree",
    "remove_working_tree",
    "io_reaping",

    "=================== IO URING ===================", 
    "uring_get_events"
};
// clang-format on

#define GREEN "\033[0;32m"
#define BLACK "\033[0m"
#define BOLD "\033[1m"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) GREEN BOLD "[IOCOMMON]: " BLACK fmt
#endif

u64 IO_Timingstats[IO_TIMING_NUM];
u64 IO_Countstats[IO_TIMING_NUM];
#ifdef __KERNEL__
DEFINE_PER_CPU(u64[IO_TIMING_NUM], IO_Timingstats_percpu);
DEFINE_PER_CPU(u64[IO_TIMING_NUM], IO_Countstats_percpu);
void io_get_timing_stats(void) {
    int i;
    int cpu;

    for (i = 0; i < IO_TIMING_NUM; i++) {
        IO_Timingstats[i] = 0;
        IO_Countstats[i] = 0;
        for_each_possible_cpu(cpu) {
            IO_Timingstats[i] += per_cpu(IO_Timingstats_percpu[i], cpu);
            IO_Countstats[i] += per_cpu(IO_Countstats_percpu[i], cpu);
        }
    }
}

static void io_clear_timing_stats(void) {
    int i;
    int cpu;

    for (i = 0; i < IO_TIMING_NUM; i++) {
        IO_Countstats[i] = 0;
        IO_Timingstats[i] = 0;
        for_each_possible_cpu(cpu) {
            per_cpu(IO_Timingstats_percpu[i], cpu) = 0;
            per_cpu(IO_Countstats_percpu[i], cpu) = 0;
        }
    }
}
#else
atomic_uint_least64_t IO_Timingstats_percpu[IO_TIMING_NUM];
atomic_uint_least64_t IO_Countstats_percpu[IO_TIMING_NUM];

void io_get_timing_stats(void) {
    int i;
    for (i = 0; i < IO_TIMING_NUM; i++) {
        IO_Timingstats[i] = atomic_load(&IO_Timingstats_percpu[i]);
        IO_Countstats[i] = atomic_load(&IO_Countstats_percpu[i]);
    }
}

static void io_clear_timing_stats(void) {
    int i;
    for (i = 0; i < IO_TIMING_NUM; i++) {
        atomic_store(&IO_Timingstats_percpu[i], 0);
        atomic_store(&IO_Countstats_percpu[i], 0);
    }
}
#endif  // __KERNEL__

void io_clear_stats(void) {
    io_clear_timing_stats();
}

int io_show_stats(void) {
    int i;

    io_get_timing_stats();

    pr_info("=========== I/O timing stats ===========\n");
    for (i = 0; i < IO_TIMING_NUM; i++) {
        /* Title */
        if (IO_Timingstring[i][0] == '=') {
            pr_info("\n");
            pr_info("%s\n\n", IO_Timingstring[i]);
            continue;
        }

        if (IO_Timingstats[i]) {
            pr_info(
                "%s: count %llu, timing %llu, average %llu\n",
                IO_Timingstring[i], IO_Countstats[i], IO_Timingstats[i],
                IO_Countstats[i] ? IO_Timingstats[i] / IO_Countstats[i] : 0);
        } else {
            pr_info("%s: count %llu\n", IO_Timingstring[i], IO_Countstats[i]);
        }
    }

    pr_info("\n");
    return 0;
}