#define _GNU_SOURCE
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <linux/io_uring.h>
#include <sys/uio.h>

#include "common.h"

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "[URING]: " fmt
#endif

#define read_barrier() __asm__ __volatile__("" ::: "memory")
#define write_barrier() __asm__ __volatile__("" ::: "memory")

struct io_sq_ring {
    unsigned *head;
    unsigned *tail;
    unsigned *ring_mask;
    unsigned *ring_entries;
    unsigned *flags;
    unsigned *array;
};

struct io_cq_ring {
    unsigned *head;
    unsigned *tail;
    unsigned *ring_mask;
    unsigned *ring_entries;
    struct io_uring_cqe *cqes;
};

struct ioring_mmap {
    void *ptr;
    size_t len;
};

enum ioring_mmap_type {
    IORING_MMAP_SQ_RING,
    IORING_MMAP_SQES,
    IORING_MMAP_CQ_RING,
    IORING_MMAP_TYPE_NUM
};

struct ioring_data {
    int ring_fd;

    char *md_buf;

    // handle of device
    int fd;

    // submit queue ring
    struct io_sq_ring sq_ring;
    // submit queue entries
    struct io_uring_sqe *sqes;
    struct iovec *iovecs;
    unsigned sq_ring_mask;

    // commit queue ring with entries
    struct io_cq_ring cq_ring;
    unsigned cq_ring_mask;

    int queued;
    int cq_ring_off;
    unsigned iodepth;
    int prepped;

    struct ioring_mmap mmap[IORING_MMAP_TYPE_NUM];
};

// map SQ ring, SQ entries, and CQ ring into user space
static int __ioring_structure_mmap(struct ioring_data *ld,
                                   struct io_uring_params *p) {
    struct io_sq_ring *sring = &ld->sq_ring;
    struct io_cq_ring *cring = &ld->cq_ring;
    void *ptr;

    ld->mmap[IORING_MMAP_SQ_RING].len =
        p->sq_off.array + p->sq_entries * sizeof(__u32);
    ptr = mmap(0, ld->mmap[IORING_MMAP_SQ_RING].len, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_POPULATE, ld->ring_fd, IORING_OFF_SQ_RING);
    ld->mmap[IORING_MMAP_SQ_RING].ptr = ptr;
    sring->head = ptr + p->sq_off.head;
    sring->tail = ptr + p->sq_off.tail;
    sring->ring_mask = ptr + p->sq_off.ring_mask;
    sring->ring_entries = ptr + p->sq_off.ring_entries;
    sring->flags = ptr + p->sq_off.flags;
    sring->array = ptr + p->sq_off.array;
    ld->sq_ring_mask = *sring->ring_mask;

    ld->mmap[IORING_MMAP_SQES].len =
        p->sq_entries * sizeof(struct io_uring_sqe);
    ld->sqes = mmap(0, ld->mmap[1].len, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_POPULATE, ld->ring_fd, IORING_OFF_SQES);
    ld->mmap[IORING_MMAP_SQES].ptr = ld->sqes;

    ld->mmap[IORING_MMAP_CQ_RING].len =
        p->cq_off.cqes + p->cq_entries * sizeof(struct io_uring_cqe);
    ptr = mmap(0, ld->mmap[2].len, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_POPULATE, ld->ring_fd, IORING_OFF_CQ_RING);
    ld->mmap[IORING_MMAP_CQ_RING].ptr = ptr;
    cring->head = ptr + p->cq_off.head;
    cring->tail = ptr + p->cq_off.tail;
    cring->ring_mask = ptr + p->cq_off.ring_mask;
    cring->ring_entries = ptr + p->cq_off.ring_entries;
    cring->cqes = ptr + p->cq_off.cqes;
    ld->cq_ring_mask = *cring->ring_mask;
    return 0;
}

static int __ioring_structure_unmap(struct ioring_data *ld) {
    int i;

    for (i = 0; i < ARRAY_SIZE(ld->mmap); i++)
        munmap(ld->mmap[i].ptr, ld->mmap[i].len);
    close(ld->ring_fd);
    return 0;
}

static int __ioring_queue_init(struct ioring_data *ld, int sqpoll_cpu) {
    struct io_uring_params p;
    int depth = ld->iodepth;
    int ret;

    memset(&p, 0, sizeof(p));

    p.flags |= IORING_SETUP_IOPOLL;
    p.flags |= IORING_SETUP_SQPOLL;
    if (sqpoll_cpu >= 0) {
        p.flags |= IORING_SETUP_SQ_AFF;
        p.sq_thread_cpu = sqpoll_cpu;
    }

    ret = syscall(__NR_io_uring_setup, depth, &p);
    if (ret < 0)
        return ret;

    ld->ring_fd = ret;

    ret = syscall(__NR_io_uring_register, ld->ring_fd, IORING_REGISTER_BUFFERS,
                  ld->iovecs, depth);
    if (ret < 0)
        return ret;

    return __ioring_structure_mmap(ld, &p);
}

static int __ioring_register_files(struct thread_data *td) {
    struct ioring_data *ld = td->io_ops_data;
    char *dev_path = td->dev_path;
    int ret;

    ld->fd = open(dev_path, O_RDWR | O_DIRECT);
    if (ld->fd < 0) {
        BUG_ON(1);
    }

    ret = syscall(__NR_io_uring_register, ld->ring_fd, IORING_REGISTER_FILES,
                  &ld->fd, 1);
    if (ret < 0) {
        BUG_ON(1);
        return ret;
    }

    return ret;
}

static int ioring_init(struct thread_data *td) {
    struct ioring_data *ld;
    int sqpoll_cpu;
    int iodepth = td->iodepth;
    int i, err;

    if (td->options)
        sqpoll_cpu = *(int *)td->options;
    else
        sqpoll_cpu = -1;

    // NOTE: calloc will initialize all memory to 0
    ld = calloc(1, sizeof(*ld));
    ld->iodepth = iodepth;

    ld->iovecs = calloc(iodepth, sizeof(struct iovec));

    td->io_ops_data = ld;

    // NOTE: fix buffers by default
    struct rlimit rlim = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim) < 0) {
        perror("setrlimit");
        return -1;
    }

    for (i = 0; i < iodepth; i++) {
        struct iovec *iovec = &ld->iovecs[i];

        iovec->iov_base = td->buf + i * td->bs;
        iovec->iov_len = td->bs;

        // pr_info("iovec %d: %p, %lu\n", i, iovec->iov_base, iovec->iov_len);
    }

    err = __ioring_queue_init(ld, sqpoll_cpu);
    if (err) {
        return err;
    }

    __ioring_register_files(td);

    return 0;
}

static int ioring_prep(struct thread_data *td, struct io_u *io_u) {
    struct ioring_data *ld = td->io_ops_data;
    unsigned long long offset = io_u->offset;
    int sqe_idx = io_u->idx;
    struct io_uring_sqe *sqe;
    u8 ioring_op_code = IORING_OP_NOP;
    sqe = &ld->sqes[sqe_idx];
    memset(sqe, 0, sizeof(*sqe));

    switch (io_u->opcode & IO_OP_MASK) {
        case IO_READ:
            ioring_op_code = IORING_OP_READ_FIXED;
            break;
        case IO_WRITE:
            ioring_op_code = IORING_OP_WRITE_FIXED;
            break;
        case IO_SYNC:
            ioring_op_code = IORING_OP_FSYNC;
            break;
        default:
            BUG_ON(1);
    }

    // NOTE: we only support one raw device file
    sqe->fd = 0;
    sqe->flags = IOSQE_FIXED_FILE;

    if (ioring_op_code == IORING_OP_FSYNC) {
        sqe->opcode = ioring_op_code;
        sqe->addr = 0;
        sqe->len = 0;
        sqe->off = 0;
        sqe->buf_index = 0;
        sqe->user_data = (unsigned long long)io_u;
    } else {
        sqe->opcode = ioring_op_code;
        assert(ld->iovecs[sqe_idx].iov_base == io_u->buf);
        sqe->addr = (unsigned long)io_u->buf;
        sqe->len = ld->iovecs[sqe_idx].iov_len;
        sqe->off = offset;
        sqe->buf_index = sqe_idx;
        // For notify kernel that `io_u` is done
        sqe->user_data = (unsigned long long)io_u;
    }

    return 0;
}

/**
 * @brief put a I/O request into the queue
 *
 * @param td thread data
 * @param offset the offset of the device to read/write (we assume that the
 * offset is aligned to 4K)
 * @param sqe_idx which sqe entry to use
 * @param opcode IORING_OP_READ_FIXED, IORING_OP_WRITE_FIXED
 * @return enum q_status
 */
static enum q_status ioring_queue(struct thread_data *td, struct io_u *io_u) {
    struct ioring_data *ld = td->io_ops_data;
    struct io_sq_ring *ring = &ld->sq_ring;
    int sqe_idx = io_u->idx;
    unsigned tail, next_tail;

    if (ld->queued == ld->iodepth)
        return Q_BUSY;

    tail = *ring->tail;
    next_tail = tail + 1;
    read_barrier();

    if (next_tail == *ring->head)
        return Q_BUSY;

#ifdef DEBUG
    struct io_uring_sqe *sqe = &ld->sqes[sqe_idx];
    pr_debug("%s: sqe->fd = %d, sqe->opcode = %d, sqe->addr = %p, sqe->len = "
             "%u, sqe->buf_index = %u, ring_index = %u\n",
             __func__, sqe->fd, sqe->opcode, (void *)sqe->addr, sqe->len,
             sqe->buf_index, tail & ld->sq_ring_mask);
#endif

    /* ensure sqe stores are ordered with tail update */
    ring->array[tail & ld->sq_ring_mask] = sqe_idx;
    *ring->tail = next_tail;
    write_barrier();

    ld->queued++;
    return Q_QUEUED;
}

static int __io_uring_enter(struct ioring_data *ld, unsigned int to_submit,
                            unsigned int min_complete, unsigned int flags) {
    return syscall(__NR_io_uring_enter, ld->ring_fd, to_submit, min_complete,
                   flags, NULL, 0);
}

static int ioring_commit(struct thread_data *td) {
    struct ioring_data *ld = td->io_ops_data;
    struct io_sq_ring *ring = &ld->sq_ring;
    int ret, commit = 0;

    if (ld->queued == 0) {
        return 0;
    }

    commit = ld->queued;

    read_barrier();
    if (*ring->flags & IORING_SQ_NEED_WAKEUP) {
        assert(ld->queued == 1);
        ret = __io_uring_enter(ld, ld->queued, 0, IORING_ENTER_SQ_WAKEUP);
        if (ret < 0) {
            BUG_ON(1);
            return ret;
        }
    }

    ld->queued = 0;
    return commit;
}

static int __ioring_cqring_reap(struct thread_data *td, unsigned int events,
                                unsigned int max) {
    struct ioring_data *ld = td->io_ops_data;
    struct io_cq_ring *ring = &ld->cq_ring;
    unsigned head, reaped = 0;

    head = *ring->head;
    do {
        read_barrier();
        if (head == *ring->tail)
            break;
        reaped++;
        head++;
    } while (reaped + events < max);

    *ring->head = head;
    write_barrier();
    return reaped;
}

static struct io_u *ioring_event(struct thread_data *td, int event) {
    struct ioring_data *ld = td->io_ops_data;
    struct io_uring_cqe *cqe;
    struct io_u *io_u;
    unsigned index;

    index = (event + ld->cq_ring_off) & ld->cq_ring_mask;

    cqe = &ld->cq_ring.cqes[index];
    io_u = (struct io_u *)cqe->user_data;

    pr_debug("res: %d, io_u->opcode: %d, io_u->idx: %d, io_u->flags: %d\n",
             cqe->res, io_u->opcode, io_u->idx, io_u->flags);

    return io_u;
}

static int ioring_getevents(struct thread_data *td, unsigned int min,
                            unsigned int max) {
    struct ioring_data *ld = td->io_ops_data;
    struct io_cq_ring *ring = &ld->cq_ring;
    unsigned long tries = 0, retries = 0;
    unsigned events = 0;
    int r, ret;
    IO_INIT_TIMING(time);

    if (!ring->head)
        return 0;

    IO_START_TIMING(uring_get_events_t, time);

    assert(ring->head);
    ld->cq_ring_off = *ring->head;

retry:
    tries = 0;
    // Fast path to get events without syscall
    do {
        r = __ioring_cqring_reap(td, events, max);
        if (r) {
            events += r;
        }
        tries++;
    } while (events < min && tries < GET_EVENTS_MAX_TRIES(td));

    if (events < min) {
        // NOTE: Go to kernel and force it to complete the requests
        struct io_sq_ring *sring = &ld->sq_ring;
        read_barrier();
        if (*sring->flags & IORING_SQ_NEED_WAKEUP) {
            ret = __io_uring_enter(ld, 0, min - events, IORING_ENTER_GETEVENTS);
            if (ret < 0) {
                BUG_ON(1);
                return ret;
            }
        }
        retries++;
        goto retry;
    }
    IO_END_TIMING(uring_get_events_t, time);
    return events;
}

static void ioring_cleanup(struct thread_data *td) {
    struct ioring_data *ld = td->io_ops_data;

    if (ld) {
        __ioring_structure_unmap(ld);
        close(ld->fd);

        free(ld->iovecs);
        free(ld);
    }
}

static int ioring_test(void) {
    struct thread_data td;
    int write_iovec_idx = 0;
    int read_iovec_idx = 1;
    int ofs = 0;
    int sqpoll_cpu = 1;
    struct io_u *io_u;

    pr_info("%s called!\n", __func__);

    assert(!thread_data_init(&td, 32, 4096, "/dev/nvme0n1p1", &sqpoll_cpu));
    assert(!ioring_init(&td));

    struct io_u io_u_w = {
        .offset = ofs,
        .idx = write_iovec_idx,
        .opcode = IO_WRITE,
        .buf = td.buf + write_iovec_idx * 4096,
    };
    struct io_u io_u_r = {
        .offset = ofs,
        .idx = read_iovec_idx,
        .opcode = IO_READ,
        .buf = td.buf + read_iovec_idx * 4096,
    };

    memcpy(td.buf, "STAR", 4);
    ioring_prep(&td, &io_u_w);
    ioring_queue(&td, &io_u_w);
    ioring_commit(&td);

    int events = ioring_getevents(&td, 1, 1);
    for (int i = 0; i < events; i++) {
        io_u = ioring_event(&td, i);
        pr_info("sqe at %d finished\n", io_u->idx);
    }

    memset(td.buf, 0, 5);

    ioring_prep(&td, &io_u_r);
    ioring_queue(&td, &io_u_r);
    ioring_commit(&td);
    events = ioring_getevents(&td, 1, 1);
    for (int i = 0; i < events; i++) {
        io_u = ioring_event(&td, i);
        pr_info("sqe at %d finished\n", io_u->idx);
    }
    char buf[5] = {0};
    memcpy(buf, td.buf + read_iovec_idx * 4096, 4);
    pr_info("buf: %s\n", buf);
    BUG_ON(strcmp(buf, "STAR") != 0);

    ioring_cleanup(&td);
    thread_data_cleanup(&td);
    return 0;
}

struct ioengine_ops uring_io_ops = {
    .name = "uring",
    .init = ioring_init,
    .prep = ioring_prep,
    .cleanup = ioring_cleanup,
    .queue = ioring_queue,
    .commit = ioring_commit,
    .getevents = ioring_getevents,
    .event = ioring_event,
    .unit_test = ioring_test,
};
EXPORT_SYMBOL(uring_io_ops);