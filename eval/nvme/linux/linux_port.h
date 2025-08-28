#ifndef __LINUX_PORT_H
#define __LINUX_PORT_H

#ifndef __KERNEL__
#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/falloc.h>
#include <linux/magic.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>
#include <stdatomic.h>
#include <signal.h>
#include <execinfo.h>

#include "kernel.h"

#include "timekeeping32.h"
#include "./bitmap.h"
#include "./hashtable.h"
#include "./list.h"
#include "./llist.h"
#include "./mm_porting.h"
#include "./myrbtree.h"
#include "./radix-tree.h"
#include "./rbtree.h"
#include "./sort.h"

#define access_ok(addr, size) 1

static inline unsigned long get_seconds(void) {
    struct timespec ts;
    getrawmonotonic(&ts);
    return ts.tv_sec;
}

// Basic types
typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;
typedef __s8 s8;
typedef __s16 s16;
typedef __s32 s32;
typedef __s64 s64;

#define IS_ERR(x) ((unsigned long)(void *)(x) >= (unsigned long)-4095)
#define PTR_ERR(x) ((long)(void *)(x))

#define __aligned(x) __attribute__((aligned(x)))
// Atomic types
typedef struct atomic64 {
    u64 __aligned(8) counter;
} atomic64_t;

#ifdef __x86_64__

#define LOCK_PREFIX_HERE              \
    ".pushsection .smp_locks,\"a\"\n" \
    ".balign 4\n"                     \
    ".long 671f - .\n" /* offset */   \
    ".popsection\n"                   \
    "671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

/*
 * Non-existent functions to indicate usage errors at link time
 * (or compile-time if the compiler implements __compiletime_error().
 */
extern void __xchg_wrong_size(void)
    __compiletime_error("Bad argument size for xchg");
extern void __cmpxchg_wrong_size(void)
    __compiletime_error("Bad argument size for cmpxchg");
extern void __xadd_wrong_size(void)
    __compiletime_error("Bad argument size for xadd");
extern void __add_wrong_size(void)
    __compiletime_error("Bad argument size for add");

/*
 * Constants for operation sizes. On 32-bit, the 64-bit size it set to
 * -1 because sizeof will never return -1, thereby making those switch
 * case statements guaranteeed dead code which the compiler will
 * eliminate, and allowing the "missing symbol in the default case" to
 * indicate a usage error.
 */
#define __X86_CASE_B 1
#define __X86_CASE_W 2
#define __X86_CASE_L 4
// 64 bits
#define __X86_CASE_Q 8

/*
 * An exchange-type operation, which takes a value and a pointer, and
 * returns the old value.
 */
#define __xchg_op(ptr, arg, op, lock)                    \
    ({                                                   \
        __typeof__(*(ptr)) __ret = (arg);                \
        switch (sizeof(*(ptr))) {                        \
            case __X86_CASE_B:                           \
                asm volatile(lock #op "b %b0, %1\n"      \
                             : "+q"(__ret), "+m"(*(ptr)) \
                             :                           \
                             : "memory", "cc");          \
                break;                                   \
            case __X86_CASE_W:                           \
                asm volatile(lock #op "w %w0, %1\n"      \
                             : "+r"(__ret), "+m"(*(ptr)) \
                             :                           \
                             : "memory", "cc");          \
                break;                                   \
            case __X86_CASE_L:                           \
                asm volatile(lock #op "l %0, %1\n"       \
                             : "+r"(__ret), "+m"(*(ptr)) \
                             :                           \
                             : "memory", "cc");          \
                break;                                   \
            case __X86_CASE_Q:                           \
                asm volatile(lock #op "q %q0, %1\n"      \
                             : "+r"(__ret), "+m"(*(ptr)) \
                             :                           \
                             : "memory", "cc");          \
                break;                                   \
            default:                                     \
                __##op##_wrong_size();                   \
        }                                                \
        __ret;                                           \
    })

#define __xadd(ptr, inc, lock) __xchg_op((ptr), (inc), xadd, lock)
#define xadd(ptr, inc) __xadd((ptr), (inc), LOCK_PREFIX)
/**
 * arch_atomic64_add_return - add and return
 * @i: integer value to add
 * @v: pointer to type atomic64_t
 *
 * Atomically adds @i to @v and returns @i + @v
 */
static __always_inline long arch_atomic64_add_return(long i, atomic64_t *v) {
    return i + xadd(&v->counter, i);
}

/**
 * arch_atomic64_set - set atomic64 variable
 * @v: pointer to type atomic64_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
static inline void arch_atomic64_set(atomic64_t *v, long i) {
    WRITE_ONCE(v->counter, i);
}

/**
 * arch_atomic64_read - read atomic64 variable
 * @v: pointer of type atomic64_t
 *
 * Atomically reads the value of @v.
 * Doesn't imply a read memory barrier.
 */
static inline long arch_atomic64_read(const atomic64_t *v) {
    return READ_ONCE((v)->counter);
}
#else
#error "Unsupported architecture"
#endif

static inline s64 atomic64_add_return(s64 i, atomic64_t *v) {
    return arch_atomic64_add_return(i, v);
}

static inline void atomic64_set(atomic64_t *v, s64 i) {
    arch_atomic64_set(v, i);
}

static inline s64 atomic64_read(const atomic64_t *v) {
    return arch_atomic64_read(v);
}

#define atomic64_read atomic64_read
#define atomic64_set atomic64_set
#define atomic64_add_return atomic64_add_return
#define atomic64_add_return_relaxed atomic64_add_return
#define ATOMIC64_INIT(i) \
    { (i) }

typedef pthread_spinlock_t spinlock_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_rwlock_t rw_semaphore_t;

#define mutex_init(mutex) pthread_mutex_init((mutex), NULL)
#define mutex_lock(mutex) pthread_mutex_lock((mutex))
#define mutex_unlock(mutex) pthread_mutex_unlock((mutex))
#define mutex_trylock(mutex) pthread_mutex_trylock((mutex))

#define spin_lock_init(lock) pthread_spin_init((lock), PTHREAD_PROCESS_PRIVATE)
#define spin_lock(lock) pthread_spin_lock((lock))
#define spin_unlock(lock) pthread_spin_unlock((lock))
#define spin_trylock(lock) pthread_spin_trylock((lock))

#define init_rwsem(sem) pthread_rwlock_init((sem), NULL)
#define down_read(sem) pthread_rwlock_rdlock((sem))
#define down_write(sem) pthread_rwlock_wrlock((sem))
#define up_read(sem) pthread_rwlock_unlock((sem))
#define up_write(sem) pthread_rwlock_unlock((sem))

struct task_struct {
    struct list_head list;
    atomic64_t should_stop;
    pthread_t t;
};

static LIST_HEAD(tasks);

static inline struct task_struct *kthread_create(int (*threadfn)(void *data),
                                                 void *data,
                                                 const char *namefmt, ...) {
    struct task_struct *task;
    task = malloc(sizeof(struct task_struct));
    pthread_create(&task->t, NULL, (void *(*)(void *))threadfn, data);
    list_add_tail(&task->list, &tasks);
    return task;
}

static inline bool kthread_should_stop(void) {
    pthread_t t = pthread_self();
    struct task_struct *task;
    list_for_each_entry(task, &tasks, list) {
        if (pthread_equal(task->t, t)) {
            return !!atomic64_read(&task->should_stop);
        }
    }
    return false;
}

static inline int kthread_stop(struct task_struct *k) {
    atomic64_set(&k->should_stop, 1);
    pthread_join(k->t, NULL);
    list_del(&k->list);
    free(k);
    return 0;
}

#define S_SYNC 1         /* Writes are synced at once */
#define S_NOATIME 2      /* Do not update access times */
#define S_APPEND 4       /* Append-only file */
#define S_IMMUTABLE 8    /* Immutable file */
#define S_DEAD 16        /* removed, but still open directory */
#define S_NOQUOTA 32     /* Inode is not counted to quota */
#define S_DIRSYNC 64     /* Directory modifications are synchronous */
#define S_NOCMTIME 128   /* Do not update file c/mtime */
#define S_SWAPFILE 256   /* Do not truncate: swapon got its bmaps */
#define S_PRIVATE 512    /* Inode is fs-internal */
#define S_IMA 1024       /* Inode has an associated IMA struct */
#define S_AUTOMOUNT 2048 /* Automount/referral quasi-directory */
#define S_NOSEC 4096     /* no suid or xattr security attributes */
#define S_DAX 8192       /* Direct Access, avoiding the page cache */

/* legacy typedef, should eventually be removed */
typedef void *fl_owner_t;

struct qstr {
    u32 hash;
    u32 len;
    const unsigned char *name;
};

#define QSTR_INIT(n, l) \
    { .len = l, .name = n }

static struct qstr slash_name = QSTR_INIT((const unsigned char *)"/", 1);

struct dentry {
    struct dentry *d_parent; /* parent directory */
    struct qstr d_name;
    struct inode *d_inode;    /* Where the name belongs to - NULL is
                               * negative */
    struct super_block *d_sb; /* The root of the dentry tree */
    struct hlist_node d_hash; /* lookup hash list */

    struct list_head d_child;   /* child of parent list */
    struct list_head d_subdirs; /* our children */
};

struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

struct file {
    struct path f_path;
    struct inode *f_inode; /* cached value */
    unsigned int f_flags;
    const struct file_operations *f_op;
    loff_t f_pos;
    void *private_data;
};

struct dir_context;
typedef int (*filldir_t)(struct dir_context *, const char *, int, loff_t, u64,
                         unsigned);
struct dir_context {
    filldir_t actor;
    loff_t pos;
};

static inline struct inode *file_inode(const struct file *f) {
    return f->f_inode;
}

struct file_operations {
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*flush)(struct file *, fl_owner_t id);
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
    int (*iterate)(struct file *, struct dir_context *);
};

struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*evict_inode)(struct inode *);
};

struct super_block {
    struct super_operations *s_op;
    struct dentry *s_root;
    void *s_fs_info;
    unsigned char s_blocksize_bits;
    unsigned long s_blocksize;
};

typedef unsigned int kuid_t;
typedef unsigned int kgid_t;

typedef unsigned short umode_t;

#define __I_NEW 3
#define I_NEW (1 << __I_NEW)

struct inode {
    ino_t i_ino;
    umode_t i_mode;
    unsigned short i_opflags;
    kuid_t i_uid;
    struct super_block *i_sb;
    const struct inode_operations *i_op;
    const struct file_operations *i_fop;
    kgid_t i_gid;
    unsigned int i_flags;
    dev_t i_rdev;
    loff_t i_size;
    struct timespec i_atime;
    struct timespec i_mtime;
    struct timespec i_ctime;
    __u32 i_generation;
    spinlock_t i_lock;
    void *i_private;
    unsigned long i_state;
    rw_semaphore_t i_rwsem;
    union {
        const unsigned int i_nlink;
        unsigned int __i_nlink;
    };
    union {
        // struct pipe_inode_info *i_pipe;
        // struct cdev *i_cdev;
        char *i_link;
        unsigned i_dir_seq;
    };
    unsigned long i_blocks;
};

struct delayed_call {
    void (*fn)(void *);
    void *arg;
};

#define timespec64 timespec

/*
 * Attribute flags.  These should be or-ed together to figure out what
 * has been changed!
 */
#define ATTR_MODE (1 << 0)
#define ATTR_UID (1 << 1)
#define ATTR_GID (1 << 2)
#define ATTR_SIZE (1 << 3)
#define ATTR_ATIME (1 << 4)
#define ATTR_MTIME (1 << 5)
#define ATTR_CTIME (1 << 6)
#define ATTR_ATIME_SET (1 << 7)
#define ATTR_MTIME_SET (1 << 8)
#define ATTR_FORCE (1 << 9) /* Not a change, but a change it */
#define ATTR_KILL_SUID (1 << 11)
#define ATTR_KILL_SGID (1 << 12)
#define ATTR_FILE (1 << 13)
#define ATTR_KILL_PRIV (1 << 14)
#define ATTR_OPEN (1 << 15) /* Truncating from open(O_TRUNC) */
#define ATTR_TIMES_SET (1 << 16)
#define ATTR_TOUCH (1 << 17)

/*
 * This is the Inode Attributes structure, used for notify_change().  It
 * uses the above definitions as flags, to know which values have changed.
 * Also, in this manner, a Filesystem can look at only the values it cares
 * about.  Basically, these are the attributes that the VFS layer can
 * request to change from the FS layer.
 *
 * Derek Atkins <warlord@MIT.EDU> 94-10-20
 */
struct iattr {
    unsigned int ia_valid;
    umode_t ia_mode;
    kuid_t ia_uid;
    kgid_t ia_gid;
    loff_t ia_size;
    struct timespec64 ia_atime;
    struct timespec64 ia_mtime;
    struct timespec64 ia_ctime;

    /*
     * Not an attribute, but an auxiliary info for filesystems wanting to
     * implement an ftruncate() like method.  NOTE: filesystem should
     * check for (ia_valid & ATTR_FILE), and not for (ia_file != NULL).
     */
    struct file *ia_file;
};

struct inode_operations {
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);
    const char *(*get_link)(struct dentry *, struct inode *,
                            struct delayed_call *);

    int (*readlink)(struct dentry *, char __user *, int);

    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*mknod)(struct inode *, struct dentry *, umode_t, dev_t);
    int (*rename)(struct inode *, struct dentry *, struct inode *,
                  struct dentry *, unsigned int);

    int (*setattr)(struct dentry *, struct iattr *);
};

static inline int setattr_prepare(struct dentry *dentry, struct iattr *attr) {
    return 0;
}

static inline void setattr_copy(struct inode *inode, const struct iattr *attr) {
    unsigned int ia_valid = attr->ia_valid;

    if (ia_valid & ATTR_ATIME)
        inode->i_atime = attr->ia_atime;
    if (ia_valid & ATTR_MTIME)
        inode->i_mtime = attr->ia_mtime;
    if (ia_valid & ATTR_CTIME)
        inode->i_ctime = attr->ia_ctime;
    if (ia_valid & ATTR_MODE) {
        umode_t mode = attr->ia_mode;
        inode->i_mode = mode;
    }
}

static inline void inode_dio_wait(struct inode *inode) {
    return;
}

static inline void inode_init_once(struct inode *inode) {
    memset(inode, 0, sizeof(*inode));

    getrawmonotonic(&inode->i_atime);
    getrawmonotonic(&inode->i_mtime);
    getrawmonotonic(&inode->i_ctime);
}

static struct dentry *d_alloc(struct dentry *parent, const struct qstr *name) {
    struct dentry *dentry = kmalloc(sizeof(struct dentry), GFP_KERNEL);
    memset(dentry, 0, sizeof(*dentry));
    dentry->d_parent = parent;

    if (unlikely(!name)) {
        dentry->d_name = slash_name;
    } else {
        dentry->d_name = *name;
    }

    dentry->d_inode = NULL;

    INIT_LIST_HEAD(&dentry->d_subdirs);
    INIT_HLIST_NODE(&dentry->d_hash);

    return dentry;
}

static inline void dput(struct dentry *dentry) {
    if (dentry) {
        kfree(dentry);
    }
}

static inline struct dentry *d_splice_alias(struct inode *inode,
                                            struct dentry *dentry) {
    dentry->d_inode = inode;
    return NULL;
}

static inline struct inode *alloc_inode(struct super_block *sb) {
    struct inode *inode = sb->s_op->alloc_inode(sb);
    if (!inode) {
        return NULL;
    }

    memset(inode, 0, sizeof(*inode));
    spin_lock_init(&inode->i_lock);
    init_rwsem(&inode->i_rwsem);
    inode->i_sb = sb;
    return inode;
}

static inline struct inode *new_inode(struct super_block *sb) {
    struct inode *inode;

    inode = alloc_inode(sb);
    return inode;
}

static inline void inode_lock(struct inode *inode) {
    down_write(&inode->i_rwsem);
}

static inline void inode_unlock(struct inode *inode) {
    up_write(&inode->i_rwsem);
}

static inline void inode_lock_shared(struct inode *inode) {
    down_read(&inode->i_rwsem);
}

static inline void inode_unlock_shared(struct inode *inode) {
    up_read(&inode->i_rwsem);
}

static inline void sb_start_write(struct super_block *sb) {
    (void)sb;
    return;
}

static inline void sb_end_write(struct super_block *sb) {
    (void)sb;
    return;
}

static inline void inode_init_owner(struct inode *inode,
                                    const struct inode *dir, umode_t mode) {
    // inode_fsuid_set(inode, mnt_userns);
    if (dir && dir->i_mode & S_ISGID) {
        inode->i_gid = dir->i_gid;

        /* Directories are special, and always inherit S_ISGID */
        if (S_ISDIR(mode))
            mode |= S_ISGID;
    }
    // else inode_fsgid_set(inode, mnt_userns);
    inode->i_mode = mode;
}

static inline struct timespec current_time(struct inode *inode) {
    struct timespec now;
    getrawmonotonic(&now);

    if (unlikely(!inode->i_sb)) {
        return now;
    }

    return now;
}

static inline void d_instantiate(struct dentry *dentry, struct inode *inode) {
    dentry->d_inode = inode;
}

static inline struct inode *iget_locked(struct super_block *sb,
                                        unsigned long ino) {
    struct inode *inode = alloc_inode(sb);

    spin_lock(&inode->i_lock);
    // TODO: handle reopen?
    inode->i_state |= I_NEW;
    inode->i_ino = ino;
    return inode;
}

static inline void inode_has_no_xattr(struct inode *inode) {
    return;
}

static inline void unlock_new_inode(struct inode *inode) {
    spin_unlock(&inode->i_lock);
}

static inline int insert_inode_locked(struct inode *inode) {
    // TODO: handle insert?
    spin_lock(&inode->i_lock);
    return 0;
}

static inline void file_accessed(struct file *file) {
}

static inline unsigned long __must_check __clear_user(void __user *addr,
                                                      unsigned long n) {
    memset(addr, 0, n);
    return n;
}

static __always_inline __must_check unsigned long __copy_to_user(
    void __user *to, const void *from, unsigned long n) {
    memcpy(to, from, n);
    return 0;
}
/**
 * set_nlink - directly set an inode's link count
 * @inode: inode
 * @nlink: new nlink (should be non-zero)
 *
 * This is a low-level filesystem helper to replace any
 * direct filesystem manipulation of i_nlink.
 */
static inline void set_nlink(struct inode *inode, unsigned int nlink) {
    if (!nlink) {
        if (inode->i_nlink) {
            inode->__i_nlink = 0;
            // atomic_long_inc(&inode->i_sb->s_remove_count);
        }
    } else {
        /* Yes, some filesystems do change nlink from zero to one */
        // if (inode->i_nlink == 0)
        //     atomic_long_dec(&inode->i_sb->s_remove_count);

        inode->__i_nlink = nlink;
    }
}

static inline void make_bad_inode(struct inode *inode) {
    inode->i_op = NULL;
    inode->i_fop = NULL;
    inode->i_state = 0;
}

static inline void init_special_inode(struct inode *inode, umode_t mode,
                                      dev_t rdev) {
    inode->i_mode = mode;
    inode->i_rdev = rdev;
    inode->i_fop = NULL;
}

static inline void iput(struct inode *inode) {
    struct super_operations *ops = inode->i_sb->s_op;

    if (ops->evict_inode)
        ops->evict_inode(inode);
    // user defined release inode
    if (ops->destroy_inode)
        ops->destroy_inode(inode);
}

static inline void iget_failed(struct inode *inode) {
    unlock_new_inode(inode);
    iput(inode);
}

static inline struct dentry *d_make_root(struct inode *root_inode) {
    struct dentry *res = NULL;
    if (root_inode) {
        res = d_alloc(NULL, NULL);
        if (res)
            d_instantiate(res, root_inode);
        else
            iput(root_inode);
    }
    return res;
}

static inline loff_t i_size_read(const struct inode *inode) {
    return inode->i_size;
}

static inline void i_size_write(struct inode *inode, loff_t i_size) {
    inode->i_size = i_size;
}

static inline int file_remove_privs(struct file *filp) {
    return 0;
}

static inline bool dir_emit_dots(struct file *file, struct dir_context *ctx) {
    return true;
}

static inline bool dir_emit(struct dir_context *ctx, const char *name,
                            int namelen, u64 ino, unsigned type) {
    return ctx->actor(ctx, name, namelen, ctx->pos, ino, type);
}

static loff_t generic_file_llseek_size(struct file *file, loff_t offset,
                                       int whence, loff_t maxsize, loff_t eof) {
    switch (whence) {
        case SEEK_END:
            offset += eof;
            break;
        case SEEK_CUR:
            /*
             * Here we special-case the lseek(fd, 0, SEEK_CUR)
             * position-querying operation.  Avoid rewriting the "same"
             * f_pos value back to the file because a concurrent read(),
             * write() or lseek() might have altered it
             */
            if (offset == 0)
                return file->f_pos;
            offset += file->f_pos;
            return offset;
    }
    file->f_pos = offset;

    return offset;
}

static inline loff_t generic_file_llseek(struct file *file, loff_t offset,
                                         int whence) {
    struct inode *inode = file->f_inode;

    return generic_file_llseek_size(file, offset, whence, 0, inode->i_size);
}

/*
 * Called when an inode is about to be open.
 * We use this to disallow opening large files on 32bit systems if
 * the caller didn't specify O_LARGEFILE.  On 64bit systems we force
 * on this flag in sys_open.
 */
static inline int generic_file_open(struct inode *inode, struct file *filp) {
    return 0;
}

static inline struct file *alloc_file(const struct path *path, int flags,
                                      const struct file_operations *fop) {
    struct file *file;

    file = kmalloc(sizeof(*file), GFP_KERNEL);
    if (IS_ERR(file))
        return file;

    file->f_path = *path;
    file->f_inode = path->dentry->d_inode;

    file->f_flags = flags;
    file->f_op = fop;
    file->f_pos = 0;
    return file;
}

static inline void fput(struct file *file) {
    if (file) {
        kfree(file);
    }
}

#define schedule() sched_yield()

#define __stringify_1(x...) #x
#define __stringify(x...) __stringify_1(x)

#define pr_fmt(fmt) fmt
// color: info: none, warn: yellow, error: red
#define pr_info(s, args...) printf("INFO (%d) " pr_fmt(s), getpid(), ##args)
#define pr_warn(s, args...)                             \
    printf("\033[0;33m"                                 \
           "DEBUG (%d-%ld %s:%d) " pr_fmt(s) "\033[0m", \
           getpid(), syscall(SYS_gettid), __FILE__, __LINE__, ##args)
#define pr_error(s, args...) printf("\033[0;31m" s "\033[0m", ##args)

#define GREEN "\033[0;32m"
#define BLACK "\033[0m"
#define BOLD "\033[1m"

#define pr_milestone(fmt, ...)         \
    printf(GREEN BOLD "["              \
                      "MILESTONE"      \
                      "]: " fmt BLACK, \
           ##__VA_ARGS__)

#ifdef DEBUG
#define pr_debug(s, args...)                            \
    printf("\033[0;31m"                                 \
           "DEBUG (%d-%ld %s:%d) " pr_fmt(s) "\033[0m", \
           getpid(), syscall(SYS_gettid), __FILE__, __LINE__, ##args)
#else
#define pr_debug(s, args...)
#endif

// Get rid of the annoying implicit declaration warning
extern int sched_getcpu(void);

static inline int smp_processor_id(void) {
    return sched_getcpu();
}

static inline int num_online_cpus(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

#endif  // __KERNEL__

#endif  // __LINUX_PORT_H