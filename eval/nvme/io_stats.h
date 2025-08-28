/*
 * HUNTER File System statistics
 *
 * Copyright 2022-2023 Regents of the University of Harbin Institute of
 * Technology, Shenzhen Computer science and technology, Yanqi Pan
 * <deadpoolmine@qq.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _IO_STATS_H
#define _IO_STATS_H

#include "./linux/linux_port.h"
#include <linux/io_uring.h>

/* ======================= Timing ========================= */
enum io_timing_category {
    _io_title_t,
    get_io_u_t,
    insert_tree_t,
    remove_tree_t,
    reap_t,

    _uring_title_t,
    uring_get_events_t,
    
    /* Sentinel */
    IO_TIMING_NUM,
};

extern const char *IO_Timingstring[IO_TIMING_NUM];
extern u64 IO_Timingstats[IO_TIMING_NUM];
extern u64 IO_Countstats[IO_TIMING_NUM];

#ifdef __KERNEL___
DECLARE_PER_CPU(u64[IO_TIMING_NUM], IO_Timingstats_percpu);
DECLARE_PER_CPU(u64[IO_TIMING_NUM], IO_Countstats_percpu);

#define IO_END_TIMING(name, start)                                    \
    {                                                                 \
        if (measure_timing) {                                         \
            IO_INIT_TIMING(end);                                      \
            getrawmonotonic(&end);                                    \
            __this_cpu_add(IO_Timingstats_percpu[name],               \
                           (end.tv_sec - start.tv_sec) * 1000000000 + \
                               (end.tv_nsec - start.tv_nsec));        \
            __this_cpu_add(IO_Countstats_percpu[name], 1);            \
        }                                                             \
    }
#else
extern atomic_uint_least64_t IO_Timingstats_percpu[IO_TIMING_NUM];
extern atomic_uint_least64_t IO_Countstats_percpu[IO_TIMING_NUM];

#define IO_END_TIMING(name, start)                                      \
    {                                                                   \
        if (io_measure_timing) {                                           \
            IO_INIT_TIMING(end);                                        \
            getrawmonotonic(&end);                                      \
            atomic_fetch_add(&IO_Timingstats_percpu[name],              \
                             (end.tv_sec - start.tv_sec) * 1000000000 + \
                                 (end.tv_nsec - start.tv_nsec));        \
            atomic_fetch_add(&IO_Countstats_percpu[name], 1);           \
        }                                                               \
    }
#endif

#define IO_INIT_TIMING(X) struct timespec X = {0}

#define IO_START_TIMING(name, start) \
    {                                \
        if (io_measure_timing)          \
            getrawmonotonic(&start); \
    }

void io_clear_stats(void);
int io_show_stats(void);

#endif /* _IO_STATS_H */
