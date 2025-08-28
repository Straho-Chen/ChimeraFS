#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "linux/kernel.h"

#define PREDEFINED_PAGE_SIZE 4096
#define PREDEFINED_CACHE_LINE_SIZE 64
#define CACHE_LINE_FILE \
    "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size"

#define GREEN "\033[0;32m"
#define BLACK "\033[0m"
#define BOLD "\033[1m"

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) GREEN BOLD "[IOCOMMON]: " BLACK fmt
#endif

#define _mm_clwb(addr) \
    asm volatile(".byte 0x66; xsaveopt %0" : "+m"(*(volatile char *)(addr)))

static inline void mem_flush(void *addr, size_t len) {
    size_t i;
    for (i = 0; i < len; i += 64) {
        _mm_clwb((char *)addr + i);
    }
}

void memcpy_nt(char *dst, char *src, unsigned size) {
    char *dst_end = dst + size;
    while (dst != dst_end) {
        __m128i res = _mm_stream_load_si128((__m128i *)src);
        *((__m128i *)dst) = res;
        src += 16;
        dst += 16;
    }
}

void memset_nt(char *dst, int c, unsigned size) {
    char *dst_end = dst + size;
    __m128i c16 = _mm_set1_epi8(c);
    while (dst != dst_end) {
        _mm_stream_si128((__m128i *)dst, c16);
        dst += 16;
    }
}

int io_measure_timing = 1;
// ==================== utils ====================
static inline int arch_cache_line_size(void) {
    char size[32];
    int fd, ret;

    fd = open(CACHE_LINE_FILE, O_RDONLY);
    if (fd < 0)
        return -1;

    ret = read(fd, size, sizeof(size));

    close(fd);

    if (ret <= 0)
        return -1;
    else
        return atoi(size);
}

static inline int os_cache_line_size(void) {
    static int size = 0;

    size = arch_cache_line_size();
    if (size <= 0)
        size = PREDEFINED_CACHE_LINE_SIZE;

    return size;
}

static inline int os_page_size(void) {
    static int size = 0;

    size = getpagesize();
    if (size <= 0)
        size = PREDEFINED_PAGE_SIZE;

    return size;
}

#define POLY 0x82f63b78
uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len) {
    int k;

    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

// ==================== ioengine helpers ====================
static int __io_check_in_range(struct io_u *io_u, off_t offset, size_t len) {
    pr_debug("io_u->offset: %ld, io_u->cap: %ld, offset: %ld, len: %ld\n",
             io_u->offset, io_u->cap, offset, len);
    return io_u->offset >= offset && io_u->offset + io_u->cap <= offset + len;
}

static int __io_check_overlapped(struct io_u *io_u, off_t offset, size_t len) {
    pr_debug("io_u->offset: %ld, io_u->cap: %ld, offset: %ld, len: %ld\n",
             io_u->offset, io_u->cap, offset, len);
    return io_u->offset + io_u->cap > offset && offset + len > io_u->offset;
}

static int __insert_working_tree(struct thread_data *td, struct io_u *io_u,
                                 struct io_u **exist) {
    struct io_u *cur;
    struct rb_node **temp, *parent = NULL;
    long compVal;

    temp = &td->working_tree.rb_node;
    if (exist)
        *exist = NULL;
    while (*temp) {
        cur = container_of(*temp, struct io_u, rb_node);
        parent = *temp;
        compVal = io_u->offset - cur->offset;
        if (compVal < 0)
            temp = &parent->rb_left;
        else if (compVal > 0)
            temp = &parent->rb_right;
        else {
            if (exist)
                *exist = cur;
            assert(cur->offset == io_u->offset);
            pr_debug("%s: %ld exists\n", __func__, io_u->offset);
            return -EINVAL;
        }
    }

    rb_link_node(&io_u->rb_node, parent, temp);
    rb_insert_color(&io_u->rb_node, &td->working_tree);

    return 0;
}

static int __search_working_tree(struct thread_data *td, off_t offset,
                                 struct io_u **io_u) {
    struct io_u *cur;
    struct rb_node *temp;
    int compVal;

    temp = td->working_tree.rb_node;
    while (temp) {
        cur = container_of(temp, struct io_u, rb_node);
        compVal = offset - cur->offset;
        if (compVal < 0)
            temp = temp->rb_left;
        else if (compVal > 0)
            temp = temp->rb_right;
        else {
            *io_u = cur;
            return 1;
        }
    }

    return 0;
}

static int __remove_working_tree(struct thread_data *td, struct io_u *io_u) {
    pr_debug("%s: io_u->idx %d, (%ld)\n", __func__, io_u->idx, io_u->offset);
#ifdef DEBUG
    int ret;
    ret = __search_working_tree(td, io_u->offset, &io_u);
    if (!ret) {
        pr_debug("%s: %ld not found\n", __func__, io_u->offset);
        return -EINVAL;
    }
#endif
    rb_erase(&io_u->rb_node, &td->working_tree);
    return 0;
}

static int __mark_io_u_available(struct thread_data *td, struct io_u *io_u) {
    assert(!!test_bit(io_u->idx, td->avai_bm) == 0);
    set_bit(io_u->idx, td->avai_bm);
#ifdef DEBUG
    pr_debug("%s: %d\n", __func__, io_u->idx);
#endif
    return 0;
}

static int __mark_io_u_working(struct thread_data *td, struct io_u *io_u) {
    clear_bit(io_u->idx, td->avai_bm);
    return 0;
}

#define REAP_MIN_AUTO -1
#define REAP_MAX_AUTO -1

static int __io_reap(struct thread_data *td, int min, int max,
                     int (*process)(struct thread_data *, struct io_u *,
                                    void *data),
                     void *data) {
    int r, i, ret;
    struct io_u *io_u;
    IO_INIT_TIMING(time);

    IO_START_TIMING(reap_t, time);

    assert(td->io_ops->commit);
    // Wake up the sq thread
    ret = td->io_ops->commit(td);
    td->inflight += ret;
    // We should have committed
    // io_u immediately after
    // __io_queue
    assert(ret == 0);

    min = min == REAP_MIN_AUTO ? ret : min;
    max = max == REAP_MAX_AUTO ? ret : max;

    assert(td->io_ops->getevents);

    r = td->io_ops->getevents(td, min, max);
    assert(r >= min);
    td->inflight -= r;

    assert(td->io_ops->event);
    for (i = 0; i < r; i++) {
        io_u = td->io_ops->event(td, i);
        if (process) {
            ret = process(td, io_u, data);
            if (ret) {
                pr_warn("%s: process failed\n", __func__);
                return ret;
            }
        }
    }

    IO_END_TIMING(reap_t, time);

    return r;
}

static int __io_queue(struct thread_data *td, int io_u_idx) {
    struct io_u *io_u = &td->io_us[io_u_idx];
    enum q_status state;
    int retries = 0;
    int queued = 0;

    assert(td->io_ops->prep);
    td->io_ops->prep(td, io_u);

    assert(td->io_ops->queue);
retry:
    state = td->io_ops->queue(td, io_u);

    assert(td->io_ops->commit);
    queued = td->io_ops->commit(td);
    assert(queued == 1);

    if (unlikely(retries > QUEUE_MAX_TRIES(td))) {
        // TODO: force commit?
        BUG_ON(1);
    }
    if (state == Q_BUSY) {
        assert(td->io_ops->commit);
        td->io_ops->commit(td);
        schedule();
        retries++;
        pr_warn("%s: queue busy, retry %d\n", __func__, retries);
        goto retry;
    }

    td->inflight++;

    return queued;
}

struct io_reap_data {
    struct list_head io_us;
    void *data;
};

struct io_reap_wrapper {
    struct list_head list;
    struct io_u *io_u;
};

static inline int __io_reap_data_init(struct io_reap_data *d) {
    INIT_LIST_HEAD(&d->io_us);
    return 0;
}

static inline int __io_reap_data_cleanup(struct io_reap_data *d) {
    struct io_reap_wrapper *w, *tmp;
    list_for_each_entry_safe(w, tmp, &d->io_us, list) {
        list_del(&w->list);
        free(w);
    }
    return 0;
}

static int __io_reap_for_nonread(struct thread_data *td, struct io_u *io_u,
                                 void *data) {
    assert(io_u->opcode != IO_READ);
    if (io_u->flags & O_IO_DROP) {
        __mark_io_u_available(td, io_u);
        __remove_working_tree(td, io_u);
    }
    return 0;
}

static int __io_reap_for_get_io_u(struct thread_data *td, struct io_u *io_u,
                                  void *data) {
    struct io_reap_data *d = data;
    struct io_reap_wrapper *w;

    if (io_u->flags & O_IO_DROP) {
        __mark_io_u_available(td, io_u);
        __remove_working_tree(td, io_u);
    }

    if (!list_empty(&d->io_us)) {
        w = calloc(1, sizeof(struct io_reap_wrapper));
        w->io_u = io_u;
        list_add_tail(&w->list, &d->io_us);
    }

    return 0;
}

struct io_read_data {
    char *buf;
    size_t len;
    size_t offset;
};

static int __io_reap_for_reader(struct thread_data *td, struct io_u *io_u,
                                void *data) {
    struct io_read_data *d = ((struct io_reap_data *)data)->data;
    char *buf = d->buf;
    size_t req_len = d->len;
    size_t req_offset = d->offset;
    size_t bias = 0, len;

    if (io_u->flags & O_IO_DROP) {
        __mark_io_u_available(td, io_u);
        __remove_working_tree(td, io_u);
    }

    if (__io_check_overlapped(io_u, req_offset, req_len)) {
        bias = req_offset - io_u->offset;
        pr_debug("%s: bias %ld\n", __func__, bias);
        if (bias > 0) {
            len = io_u->cap - bias > req_len ? req_len : io_u->cap - bias;
            memcpy(buf, io_u->buf + bias, len);
        } else {
            len = bias + req_len > io_u->cap ? io_u->cap : bias + req_len;
            memcpy(buf - bias, io_u->buf, len);
        }
    } else {
        pr_debug("%s: no overlap\n", __func__);
        assert(io_u->opcode == IO_WRITE);
    }

    return 0;
}

static int __io_u_avai_count(struct thread_data *td) {
    return bitmap_weight(td->avai_bm, td->iodepth);
}

static int __get_io_u(struct thread_data *td, struct io_u **io_u) {
    int avai = __io_u_avai_count(td);
    pr_debug("%s: available io_u(s): %d\n", __func__, avai);
    if (avai == 0) {
        *io_u = NULL;
        return -1;
    }

    int idx = find_first_bit(td->avai_bm, td->iodepth);
    pr_debug("%s: %d\n", __func__, idx);
    *io_u = &td->io_us[idx];

    return 0;
}

static int __get_io_u_slow_path(struct thread_data *td, struct io_u **io_u) {
    int ret = 0, r;
    struct io_reap_data d;
    struct io_reap_wrapper *w;

    pr_debug("%s\n", __func__);

    __io_reap_data_init(&d);

    r = __io_reap(td, 1, td->iodepth, __io_reap_for_get_io_u, &d);
    assert(r >= 0);

    if (list_empty(&d.io_us)) {
        // Retry
        ret = __get_io_u(td, io_u);
        if (*io_u == NULL) {
            pr_error("Failed to get io_u, there are too many cached io_u(s)"
                     "in this thread! Please use io_flush to release them\n");
            BUG_ON(1);
            return -EBUSY;
        }
    } else {
        w = list_first_entry(&d.io_us, struct io_reap_wrapper, list);
        *io_u = w->io_u;
    }

    __io_reap_data_cleanup(&d);

    return ret;
}

// ==================== ioengine ====================
int thread_data_init(struct thread_data *td, int iodepth, int bs,
                     char *dev_path, void *options) {
    size_t buf_size;
    int ret;

    buf_size = (unsigned long long)iodepth * (unsigned long long)bs;

    ret = posix_memalign(&td->buf, os_page_size(), buf_size);
    if (ret) {
        perror("posix_memalign");
        BUG_ON(1);
        return ret;
    }

    assert(bs % os_cache_line_size() == 0);

    td->iodepth = iodepth;
    td->bs = bs;
    td->dev_path = dev_path;
    td->options = options;
    td->io_ops = NULL;
    td->working_tree = RB_ROOT;
    td->inflight = 0;

    td->avai_bm =
        kzalloc(BITS_TO_LONGS(iodepth) * sizeof(unsigned long), GFP_KERNEL);
    // all io_u(s) are available
    bitmap_set(td->avai_bm, 0, iodepth);

    assert(BITS_PER_LONG == 64);
    pr_debug("%d, %d\n", __io_u_avai_count(td), iodepth);
    assert(__io_u_avai_count(td) == iodepth);

    td->io_us = calloc(iodepth, sizeof(struct io_u));
    if (!td->io_us) {
        perror("calloc");
        BUG_ON(1);
        return -ENOMEM;
    }

    for (int i = 0; i < iodepth; i++) {
        td->io_us[i].buf = td->buf + i * bs;
        td->io_us[i].cap = bs;
        td->io_us[i].idx = i;
        td->io_us[i].flags = 0;
    }

    return 0;
}

int thread_data_cleanup(struct thread_data *td) {
    free(td->avai_bm);
    free(td->buf);
    free(td->io_us);
    return 0;
}

static int get_io_u(struct thread_data *td, struct io_u **io_u) {
    int ret;
    IO_INIT_TIMING(time);

    IO_START_TIMING(get_io_u_t, time);

    ret = __get_io_u(td, io_u);
    if (*io_u == NULL) {
        ret = __get_io_u_slow_path(td, io_u);
    }
    assert(*io_u != NULL);

    __mark_io_u_working(td, *io_u);

    IO_END_TIMING(get_io_u_t, time);

    return ret;
}

static struct list_head engine_list;

static inline struct ioengine_ops *__find_ioengine(const char *name) {
    struct ioengine_ops *io_ops;

    list_for_each_entry(io_ops, &engine_list, list) {
        if (strcmp(io_ops->name, name) == 0) {
            return io_ops;
        }
    }

    return NULL;
}

static inline void register_ioengine(struct ioengine_ops *io_ops) {
    list_add_tail(&io_ops->list, &engine_list);
}

static inline void unregister_ioengine(struct ioengine_ops *io_ops) {
    list_del(&io_ops->list);
}

extern struct ioengine_ops uring_io_ops;

// Don't optimize this function
__optimize("O1") int io_register(bool unit_test) {
    struct ioengine_ops *io_ops;
    int ret = 0;

    INIT_LIST_HEAD(&engine_list);
    register_ioengine(&uring_io_ops);

    if (unit_test) {
        list_for_each_entry(io_ops, &engine_list, list) {
            if (io_ops->unit_test) {
                pr_milestone("io engine %s start test\n", io_ops->name);
                ret = io_ops->unit_test();
                if (ret) {
                    pr_info("failed\n");
                    BUG_ON(1);
                    return ret;
                }
                pr_milestone("io engine %s test end: Success\n", io_ops->name);
            }
        }
    }
    return ret;
}

int io_unregister(void) {
    unregister_ioengine(&uring_io_ops);
    return 0;
}

int io_open(struct thread_data *td, const char *e) {
    struct ioengine_ops *io_ops;

    io_ops = __find_ioengine(e);
    if (!io_ops) {
        pr_info("io engine %s not found\n", e);
        return -1;
    }

    td->io_ops = io_ops;

    assert(td->io_ops->init);
    td->io_ops->init(td);

    return 0;
}

int io_close(struct thread_data *td) {
    assert(td->io_ops->cleanup);
    td->io_ops->cleanup(td);
    td->io_ops = NULL;
    return 0;
}

int io_read(struct thread_data *td, off_t offset, char *buf, size_t len,
            int flags);

// Return partial write io_u idx (it is your choice to queue it or not)
// -1 means no partial write, all io_u(s) are queued
// -EBUSY means no io_u available
// >= 0 means partial write, the idx of the io_u that is not queued
// NOTE: if this is a partial write which has been flushed to disk,
//       caller must use a read-after-write method to ensure the data
//       is consistent.
int io_write(struct thread_data *td, off_t offset, char *buf, size_t len,
             int flags) {
    struct io_u *io_u = NULL, *exist = NULL;
    int ret = -1;
    unsigned long index;
    unsigned long start_index = offset / td->bs;
    unsigned long end_index = (offset + len - 1) / td->bs;
    size_t per_size, bias;
    bool need_queued = true;

    index = start_index;
    while (index <= end_index) {
        ret = get_io_u(td, &io_u);
        if (ret) {
            pr_info("get_io_u failed\n");
            return -EBUSY;
        }

#ifdef DEBUG
        assert(io_u != NULL);
#endif

        bias = offset & (td->bs - 1);
        per_size = len + bias > td->bs ? td->bs - bias : len;

        io_u->opcode = IO_WRITE;
        io_u->offset = round_down(offset, td->bs);
        io_u->flags = flags;

        __insert_working_tree(td, io_u, &exist);
        if (exist) {
            // check if exist is aligned
            assert((exist->offset & (td->bs - 1)) == 0);
            // check the requested io_u is in the range of exist
            assert(__io_check_in_range(io_u, exist->offset, exist->cap));
            // check if exist is cached
            assert(exist->flags & O_IO_CACHED);
            // NOTE: Please use io_drop to drop the cached io_u
            assert(flags & O_IO_CACHED || flags & O_IO_AUTO);
            __mark_io_u_available(td, io_u);
            pr_debug("HIT: %d\n", io_u->idx);
            need_queued = false;
            io_u = exist;
        } else {
            need_queued = !(flags & O_IO_CACHED);
            if (bias != 0) {
                pr_warn("Partial write without caching: %ld, %ld, try read "
                        "modify write\n",
                        io_u->offset, bias);
                io_read(td, io_u->offset, io_u->buf, td->bs, O_IO_DROP);
            }
        }
        io_u->opcode = IO_WRITE;

        // make sure the data is flushed to media
        // buffer instead of CPU cache
        if (buf)
            memcpy_nt(io_u->buf + bias, buf, per_size);
        else
            memset_nt(io_u->buf + bias, 0, per_size);

        // Queue it any way
        // But might not release even the
        // io_u is completed, see flags
        if (need_queued)
            __io_queue(td, io_u->idx);

        offset += per_size;
        len -= per_size;
        if (buf)
            buf += per_size;
        index += 1;
    }

    return 0;
}

// Must wait for all io_u(s) to be completed
// NOTE: `flags` is not used for read anymore
int io_read(struct thread_data *td, off_t offset, char *buf, size_t len,
            int flags) {
    struct io_u *io_u = NULL, *exist;
    int ret = -1, r;
    unsigned long index;
    unsigned long start_index = offset / td->bs;
    unsigned long end_index = (offset + len - 1) / td->bs;
    size_t per_size, bias;
    struct io_read_data data = {
        .buf = buf,
        .len = len,
        .offset = offset,
    };
    struct io_reap_data d = {
        .data = &data,
    };
    __io_reap_data_init(&d);

    r = end_index - start_index + 1;
    index = start_index;
    while (index <= end_index) {
        ret = get_io_u(td, &io_u);
        if (ret) {
            pr_info("get_io_u failed\n");
            return -EBUSY;
        }

        bias = offset & (td->bs - 1);
        per_size = len + bias > td->bs ? td->bs - bias : len;

        io_u->opcode = IO_READ;
        io_u->offset = rounddown(offset, td->bs);
        io_u->flags = flags;

        // fetch from cache
        __insert_working_tree(td, io_u, &exist);
        if (exist) {
            assert((exist->offset & (td->bs - 1)) == 0);
            assert(__io_check_in_range(io_u, exist->offset, exist->cap));
            assert(exist->flags & O_IO_CACHED);
            assert(exist->opcode == IO_WRITE);
            pr_debug("HIT: %ld\n", io_u->offset);
            __mark_io_u_available(td, io_u);
            // Directly read from cache
            __io_reap_for_reader(td, exist, &d);
            r--;
        } else {
            pr_debug("Queued: %ld\n", io_u->offset);
            __io_queue(td, io_u->idx);
        }

        offset += per_size;
        len -= per_size;
        buf += per_size;
        index += 1;
    }

    if (r != 0) {
        // reap write now
        r = td->inflight;
        pr_debug("%s: perform %d I/O from device\n", __func__, r);
        __io_reap(td, r, td->iodepth, __io_reap_for_reader, &d);
    }
    __io_reap_data_cleanup(&d);

    return 0;
}

struct io_u_list_wrapper {
    struct list_head list;
    struct io_u *io_u;
};

// Wait for all io_u(s) to be completed
int io_fence(struct thread_data *td) {
    struct io_u *io_u, *n;
    struct io_u_list_wrapper *lw, *lw_n;
    struct list_head entry_list = LIST_HEAD_INIT(entry_list);
    int r = 0;

    // pr_warn("%s: in flight: %d, reaping them all\n", __func__, td->inflight);

    // All the (write) I/Os are waited here (similar to a fence operation)
    r = __io_reap(td, td->inflight, td->iodepth, __io_reap_for_nonread, NULL);

    rbtree_postorder_for_each_entry_safe(io_u, n, &td->working_tree, rb_node) {
        lw = calloc(1, sizeof(struct io_u_list_wrapper));
        lw->io_u = io_u;
        list_add_tail(&lw->list, &entry_list);
    }

    // Do not invalidate the cached io_u(s) here.
    list_for_each_entry_safe(lw, lw_n, &entry_list, list) {
        if (lw->io_u->flags & O_IO_DROP) {
            __mark_io_u_available(td, lw->io_u);
            __remove_working_tree(td, lw->io_u);
        }
        list_del(&lw->list);
        free(lw);
    }

    return r;
}

// write back the cached io_u(s) to media without evicting them
int io_clwb(struct thread_data *td, off_t offset, size_t len) {
    off_t cur, aligned_offset = round_down(offset, td->bs);
    size_t aligned_len = round_up(len, td->bs);
    struct io_u *io_u;
    int r = 0;

    for (cur = aligned_offset; cur < aligned_offset + aligned_len;
         cur += td->bs) {
        if (__search_working_tree(td, cur, &io_u)) {
            r++;
            assert(io_u->flags & O_IO_CACHED);
            __io_queue(td, io_u->idx);
            pr_debug("%s: io_u->idx: %d (%ld)\n", __func__, io_u->idx,
                     io_u->offset);
        }
    }

    return r;
}

// invalidate the cached io_u(s) without write back
int io_wbinvd(struct thread_data *td, off_t offset, size_t len) {
    off_t cur, aligned_offset = round_down(offset, td->bs);
    size_t aligned_len = round_up(len, td->bs);
    struct io_u *io_u;
    int r = 0;

    for (cur = aligned_offset; cur < aligned_offset + aligned_len;
         cur += td->bs) {
        if (__search_working_tree(td, cur, &io_u)) {
            // has been fenced
            if (!(io_u->flags & O_IO_CACHED)) {
                assert(io_u->flags & O_IO_DROP);
            } else {
                r++;
                io_u->flags = O_IO_DROP;
                pr_debug("%s: io_u->idx: %d (%ld)\n", __func__, io_u->idx,
                         io_u->offset);
            }
        }
    }

    return r;
}

// evict the cached io_u(s) based on ret
int io_flush(struct thread_data *td, off_t offset, size_t len) {
    off_t cur, aligned_offset = round_down(offset, td->bs);
    size_t aligned_len = round_up(len, td->bs);
    struct io_u *io_u;
    int r = 0;

    for (cur = aligned_offset; cur < aligned_offset + aligned_len;
         cur += td->bs) {
        if (__search_working_tree(td, cur, &io_u)) {
            r++;
            assert(io_u->flags & O_IO_CACHED);
            io_u->flags = O_IO_DROP;
            __io_queue(td, io_u->idx);
            pr_debug("%s: io_u->idx: %d (%ld)\n", __func__, io_u->idx,
                     io_u->offset);
        }
    }

    return r;
}

// flush all the cached io_u(s) to media
int io_drain(struct thread_data *td) {
    struct io_u *io_u, *n;
    int r = 0;

    rbtree_postorder_for_each_entry_safe(io_u, n, &td->working_tree, rb_node) {
        if (io_u->flags & O_IO_CACHED) {
            io_u->flags = O_IO_DROP;
            __io_queue(td, io_u->idx);
            pr_debug("%s: io_u->idx: %d (%ld)\n", __func__, io_u->idx,
                     io_u->offset);
            r++;
        }
    }

    return r;
}

// ==================== unit test tools ====================
static int __test;
#define DEFINE_UNIT_TEST(name, FUNC)                                       \
    __maybe_unused static int name(struct thread_data *td, ...) {          \
        __test++;                                                          \
        pr_milestone("IO UNIT TEST [%d]: %s\n", __test, __func__);         \
        FUNC;                                                              \
        pr_milestone("IO UNIT TEST [%d]: %s Success\n", __test, __func__); \
        return 0;                                                          \
    }

#define BENCH_START(name)           \
    {                               \
        struct timespec start, end; \
        getrawmonotonic(&start);    \
        io_clear_stats();

#define BENCH_END(name)                                             \
    getrawmonotonic(&end);                                          \
    io_show_stats();                                                \
    pr_milestone("%s done in %lf s\n", name,                        \
                 (end.tv_sec - start.tv_sec) +                      \
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0); \
    }

#define IO_BENCH_START(name)        \
    {                               \
        struct timespec start, end; \
        getrawmonotonic(&start);    \
        io_clear_stats();

#define IO_BENCH_END(name, size_in_bytes)                             \
    getrawmonotonic(&end);                                            \
    io_show_stats();                                                  \
    pr_milestone("%s done in %lf s, bandwidth %lf MB/s\n", name,      \
                 (end.tv_sec - start.tv_sec) +                        \
                     (end.tv_nsec - start.tv_nsec) / 1000000000.0,    \
                 (double)((double)size_in_bytes / 1024 / 1024) /      \
                     ((end.tv_sec - start.tv_sec) +                   \
                      (end.tv_nsec - start.tv_nsec) / 1000000000.0)); \
    }

// ==================== unit test ====================
DEFINE_UNIT_TEST(__sync_read_after_write, {
    char buf[6] = {0};
    int ret;

    ret = io_write(td, 0, "uring", 5, O_IO_DROP);
    assert(!ret);
    io_fence(td);
    io_read(td, 0, buf, 5, O_IO_DROP);
    pr_info("buf %p: %s\n", buf, buf);
    if (strcmp(buf, "uring") != 0) {
        pr_warn("Failed to open uring device\n");
        BUG_ON(1);
        return -1;
    }
})

DEFINE_UNIT_TEST(__sync_bunch_read_after_bunch_write, {
    char buf[4096];
    off_t offset = 0;
    unsigned long loop = 1024 * 1024;
    unsigned long i;
    int ret;

    memset(buf, 'a', 4096);

    IO_BENCH_START("write");
    offset = 0;
    for (i = 0; i < loop; i++) {
        ret = io_write(td, offset, buf, 4096, O_IO_DROP);
        assert(!ret);
        offset += 4096;
    }
    IO_BENCH_END("write", loop * 4096);

    BENCH_START("sync");
    io_fence(td);
    BENCH_END("sync");

    assert(rb_first(&td->working_tree) == NULL);

    IO_BENCH_START("read");
    offset = 0;
    for (i = 0; i < loop; i++) {
        memset(buf, 0, 4096);
        ret = io_read(td, offset, buf, 4096, O_IO_DROP);
        assert(!ret);
        for (int j = 0; j < 4096; j++) {
            if (buf[j] != 'a') {
                pr_warn("Failed to open uring device\n");
                BUG_ON(1);
                return -1;
            }
        }
        offset += 4096;
    }
    IO_BENCH_END("read", loop * 4096);
})

DEFINE_UNIT_TEST(__async_raw_and_war, {
    char buf[6] = {0};
    int ret = 0;

    BENCH_START("raw");
    ret = io_write(td, 0, "uring", 5, O_IO_CACHED);
    assert(!ret);
    io_read(td, 0, buf, 5, O_IO_AUTO);
    pr_info("buf %p: %s\n", buf, buf);
    if (strcmp(buf, "uring") != 0) {
        pr_warn("Failed to open uring device\n");
        BUG_ON(1);
        return -1;
    }
    // now we drop the cached io_u
    io_flush(td, 0, 5);
    // wait the write to be completed
    io_fence(td);
    assert(__io_u_avai_count(td) == td->iodepth);
    assert(rb_first(&td->working_tree) == NULL);
    BENCH_END("raw");

    memset(buf, 0, 6);

    BENCH_START("war");
    // should be "uring"
    ret = io_read(td, 0, buf, 5, O_IO_CACHED);
    assert(td->inflight == 0);
    assert(rb_first(&td->working_tree) != NULL);
    assert(!ret);
    // modify the buf
    io_write(td, 0, "o", 1, O_IO_AUTO);
    // wait the write to be completed in media
    // since that we gonna drop the cached io_u
    io_flush(td, 0, 1);
    io_fence(td);
    assert(__io_u_avai_count(td) == td->iodepth);
    ret = io_read(td, 0, buf, 5, O_IO_DROP);
    assert(!ret);
    pr_info("buf %p: %s\n", buf, buf);
    if (strcmp(buf, "oring") != 0) {
        pr_warn("Failed to open uring device\n");
        BUG_ON(1);
        return -1;
    }
    BENCH_END("war");
})

DEFINE_UNIT_TEST(__wofs_bench, {
#define meta_region_start(nr) (meta_regions[nr].start_offset)

#define meta_full(nr, entry_size)          \
    (meta_regions[nr].start_offset != 0 && \
     meta_regions[nr].cur_offset ==        \
         meta_regions[nr].start_offset + meta_regions[nr].cap)

#define alloc_meta_region(nr, cur_offset, rsize, entry_size)                   \
    ({                                                                         \
        off_t ret;                                                             \
        if (meta_regions[nr].start_offset == 0 || meta_full(nr, entry_size)) { \
            meta_regions[nr].start_offset = cur_offset;                        \
            meta_regions[nr].cur_offset = cur_offset;                          \
            meta_regions[nr].cap = rsize;                                      \
            cur_offset += rsize;                                               \
        }                                                                      \
        ret = meta_regions[nr].cur_offset;                                     \
        meta_regions[nr].cur_offset += entry_size;                             \
        ret;                                                                   \
    })

#define alloc_data_block(cur_offset, bsize) \
    ({                                      \
        off_t ret;                          \
        ret = cur_offset;                   \
        cur_offset += bsize;                \
        ret;                                \
    })
    struct meta_region {
        off_t start_offset;
        off_t cur_offset;
        size_t cap;
    };

    char block_buf[4096] = {0};
    char meta_buf[256] = {0};
    char test_name[256] = {0};
    unsigned long loop = 1024 * 1024;
    unsigned long i;
    off_t cur_offset = 0;
    off_t block_offset = 0;
    off_t meta_offset = 0;
    off_t meta_offset_start = 0;
    struct meta_region *meta_regions;
    int ret;
    int max_nr_meta_regions = 5;
    int nr_meta_region;
    unsigned long flush_count = 0;
    unsigned long fence_count = 0;
    int entry_size = 64;
    int sync = 1;

    va_list args;
    va_start(args, td);
    sync = va_arg(args, int);
    va_end(args);

    pr_milestone("START WOFS %s BENCH\n", sync == 1 ? "SYNC" : "ASYNC");

    memset(block_buf, 'a', 4096);
    memset(meta_buf, 'b', entry_size);

    for (nr_meta_region = 1; nr_meta_region <= max_nr_meta_regions;
         nr_meta_region++) {
        meta_regions = calloc(nr_meta_region, sizeof(struct meta_region));
        snprintf(test_name, 256, "write w/ %d meta", nr_meta_region);

        IO_BENCH_START(test_name);
        assert(meta_regions != NULL);
        flush_count = 0;
        cur_offset = 4 * 1024 * 1024;
        fence_count = 0;
        for (i = 0; i < loop; i++) {
            block_offset = alloc_data_block(cur_offset, 4096);

            pr_debug("block write to [%ld, %ld)\n", block_offset,
                     block_offset + 4096);

            assert(block_offset >= 0);
            assert((block_offset & (4096 - 1)) == 0);

            ret = io_write(td, block_offset, block_buf, 4096, O_IO_DROP);
            assert(!ret);

            // Not random
            for (int nr = 0; nr < nr_meta_region; nr++) {
                meta_offset =
                    alloc_meta_region(nr, cur_offset, 4096, entry_size);
                pr_debug("meta write to [%ld, %ld)\n", meta_offset,
                         meta_offset + entry_size);

                // make sure the first meta region ordered with data block
                if (nr == 0) {
                    // emulate checksum
                    // Slower calculation here, the faster we can `reap`
                    // (due to async I/O)
                    // NOTE: sample some of data content and avoid double
                    //       fence.
                    ret = crc32c(~0, (const unsigned char *)meta_buf,
                                 entry_size * 2);
                    assert(ret != 0);
                }

                assert(meta_offset >= 0);
                ret = io_write(td, meta_offset, meta_buf, entry_size,
                               O_IO_CACHED);
                assert(!ret);

                if (sync == 1) {
                    flush_count += io_clwb(td, meta_offset, entry_size);
                    fence_count += io_fence(td);
                }

                if (meta_full(nr, entry_size)) {
                    meta_offset_start = meta_region_start(nr);

                    pr_debug("meta flush to [%ld, %ld)\n", meta_offset_start,
                             meta_offset_start + 4096);
                    assert((meta_offset_start & (4096 - 1)) == 0);

                    if (sync == 1) {  // sync
                        io_wbinvd(td, meta_offset_start, 4096);
                    } else {  // async
                        io_flush(td, meta_offset_start, 4096);
                        io_fence(td);
                    }
                }
            }
        }
        pr_info("flush_count: %ld, fence_count: %ld\n", flush_count,
                fence_count);
        IO_BENCH_END(test_name, loop * 4096);
        free(meta_regions);
        io_fence(td);
    }
})

DEFINE_UNIT_TEST(__urfs_bench, {

                               })

int io_test(void) {
    struct thread_data td;
    int options = -1;

    io_register(true);

    thread_data_init(&td, num_online_cpus(), 4096, "/dev/nvme0n1p1", &options);
    io_open(&td, "uring");
    // assert(!__sync_read_after_write(&td));
    // assert(!__sync_bunch_read_after_bunch_write(&td));
    // assert(!__async_raw_and_war(&td));
    assert(!__wofs_bench(&td, 1));  // sync
    assert(!__wofs_bench(&td, 0));  // async
    io_close(&td);
    thread_data_cleanup(&td);

    io_unregister();
    return 0;
}