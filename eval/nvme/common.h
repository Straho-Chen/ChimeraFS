#ifndef _COMMON_H
#define _COMMON_H

#include "./linux/linux_port.h"
#include <linux/io_uring.h>
#include "io_stats.h"

#ifndef GET_EVENTS_MAX_TRIES
#define GET_EVENTS_MAX_TRIES(td) ((td)->iodepth * 10)
#endif

#ifndef QUEUE_MAX_TRIES
#define QUEUE_MAX_TRIES(td) ((td)->iodepth)
#endif

#define IO_READ 0x1
#define IO_WRITE 0x2
#define IO_SYNC 0x4
#define IO_OP_MASK 0x7

#define O_IO_CACHED 0x1
#define O_IO_DROP 0x2
#define O_IO_AUTO 0x4

enum q_status {
    Q_COMPLETED = 0, /* completed sync */
    Q_QUEUED = 1,    /* queued, will complete async */
    Q_BUSY = 2,      /* no more room, call ->commit() */
};

struct io_u {
    struct rb_node rb_node;
    // Never changed
    void *buf;
    size_t cap;
    int idx;
    // Dynamic
    off_t offset;  // offset relative to the start of the device
    int opcode;
    int flags;
};

struct thread_data {
    int cpuid;

    int iodepth;
    int bs;
    int inflight;

    char *dev_path;

    void *buf;

    // For determining whether to fetch data from cache
    // working tree should never overlapped between io_us
    struct rb_root working_tree;
    // Bitmap for constant time lookup
    unsigned long *avai_bm;
    struct io_u *io_us;

    void *io_ops_data;
    struct ioengine_ops *io_ops;

    void *options;
};

struct ioengine_ops {
    const char *name;
    struct list_head list;

    int (*init)(struct thread_data *);
    int (*prep)(struct thread_data *, struct io_u *);
    enum q_status (*queue)(struct thread_data *, struct io_u *);
    int (*commit)(struct thread_data *);
    int (*getevents)(struct thread_data *, unsigned int, unsigned int);
    struct io_u *(*event)(struct thread_data *, int);
    void (*cleanup)(struct thread_data *);

    int (*unit_test)(void);
};

extern int io_measure_timing;

// Common
int thread_data_init(struct thread_data *td, int iodepth, int bs,
                     char *dev_path, void *options);
int thread_data_cleanup(struct thread_data *td);

// IO ops
int io_register(bool unit_test);
int io_unregister(void);

int io_open(struct thread_data *td, const char *e);
int io_close(struct thread_data *td);
int io_write(struct thread_data *td, off_t offset, char *buf, size_t len,
             int flags);
int io_read(struct thread_data *td, off_t offset, char *buf, size_t len,
            int flags);
int io_fence(struct thread_data *td);
int io_clwb(struct thread_data *td, off_t offset, size_t len);
int io_wbinvd(struct thread_data *td, off_t offset, size_t len);
int io_flush(struct thread_data *td, off_t offset, size_t len);
int io_drain(struct thread_data *td);
int io_test(void);

#endif