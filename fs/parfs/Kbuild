SRC_ROOT := .

obj-$(CONFIG_PARFS_FS)      += parfs.o
parfs-objs := $(addprefix $(SRC_ROOT)/solros_lib/, \
                 solros_ring_buffer.o \
                 ring_buffer.o)

parfs-objs += balloc.o bbuild.o checksum.o dax.o dir.o file.o gc.o inode.o ioctl.o \
	journal.o log.o mprotect.o namei.o parity.o rebuild.o snapshot.o stats.o \
	pmem_ar_block.o agent.o delegation.o ring.o super.o symlink.o sysfs.o perf.o


ccflags-y += -I$(src)/solros_include
ccflags-y += -O3 -mtune=native
EXTRA_CFLAGS += -DRING_BUFFER_CONF_KERNEL \
		-DRING_BUFFER_CONF_NO_MMAP \
		-DRING_BUFFER_CONF_NO_DOUBLE_MMAP

EXTRA_CFLAGS += -DRANGE_LOCK_SEGMENT
