#
# Makefile for the linux slfs filesystem routines.
#

obj-$(CONFIG_SLFS) += slfs.o

slfs-y := bbuild.o balloc.o dir.o file.o inode.o ioctl.o journal.o super.o xip.o

slfs-$(CONFIG_SLFS_XIP) += xip.o