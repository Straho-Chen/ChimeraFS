#ifndef _SLFS_JOURNAL_H
#define _SLFS_JOURNAL_H
#include <linux/slab.h>

/* default slfs journal size 4MB */
#define SLFS_DEFAULT_JOURNAL_SIZE (4 << 20)
/* minimum slfs journal size 64KB */
#define SLFS_MINIMUM_JOURNAL_SIZE (1 << 16)

#define CACHELINE_SIZE (64)
#define CLINE_SHIFT (6)
#define CACHELINE_MASK (~(CACHELINE_SIZE - 1))
#define CACHELINE_ALIGN(addr) (((addr) + CACHELINE_SIZE - 1) & CACHELINE_MASK)

#define LOGENTRY_SIZE CACHELINE_SIZE
#define LESIZE_SHIFT CLINE_SHIFT

#endif