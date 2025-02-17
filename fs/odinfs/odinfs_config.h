/*
 * odinfs_config.h
 *
 *  Created on: Sep 2, 2021
 *      Author: dzhou
 */

#ifndef __ODINFS_CONFIG_H_
#define __ODINFS_CONFIG_H_

#define ODINFS_DELEGATION_ENABLE 1

#define ODINFS_MAX_SOCKET 8
#define ODINFS_MAX_AGENT_PER_SOCKET 28
#define ODINFS_MAX_AGENT (ODINFS_MAX_SOCKET * ODINFS_MAX_AGENT_PER_SOCKET)

#define ODINFS_DEBUG_MEM_ERROR 0

#define ODINFS_RUN_IN_VM 0
#define ODINFS_MEASURE_TIME 1

#define ODINFS_AGENT_TASK_MAX_SIZE (8 + 1)

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when agents are serving
 * requests. The 3000 value is set with 32KB strip size where memcpy 32KB
 * takes around 70000 cycles. So (2.2*10^9) / 70000 = 3000
 */
#define ODINFS_AGENT_REQUEST_CHECK_COUNT 3000

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when agents are spinning
 * on the ring buffer
 *
 * This assumes that one ring buffer acquire operation takes 100 cycles
 */

#define ODINFS_AGENT_RING_BUFFER_CHECK_COUNT 220000

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when the application
 * threads are sending requests to the ring buffer
 *
 * This assumes that copying to the ring buffer takes around 100000 to complete
 *
 * The current ring buffer performance is unreasonable with 210+ application
 * threads, need to revise.
 */

#define ODINFS_APP_RING_BUFFER_CHECK_COUNT 220

/*
 * Do cond_schuled() every 100ms when the application thread is waiting for
 * the delegation request to complete.
 */
#define ODINFS_APP_CHECK_COUNT 220000000

#define ODINFS_SOLROS_RING_BUFFER 1

/* write delegation limits: 256 */
#define ODINFS_WRITE_DELEGATION_LIMIT 256

/* read delegation limits: 32K */
#define ODINFS_READ_DELEGATION_LIMIT (32 * 1024)

/* Number of default delegation threads per socket */
#define ODINFS_DEF_DELE_THREADS_PER_SOCKET 1

/* When set, use nt store to write to memory */
#define ODINFS_NT_STORE 0

/* 2MB */
#define ODINFS_RING_SIZE (2 * 1024 * 1024)

/*
 * Add this config to eliminate the effects of journaling when
 * study performance
 */
#define ODINFS_ENABLE_JOURNAL 1

/*
 * We test on vm(ubuntu 22.04), and range lock will call null pointer dereference
 * which will cause kernel panic. So we disable it.
 */
#define ODINFS_FINE_GRAINED_LOCK 0

/* stock inode lock in the linux kernel */
#define ODINFS_INODE_LOCK_STOCK 1
/* PCPU_RWSEM from the max paper */
#define ODINFS_INODE_LOCK_MAX_PERCPU 2
/* stock per_cpu_rwsem in the  linux kernel */
#define ODINFS_INODE_LOCK_PERCPU 3

#define ODINFS_INODE_LOCK ODINFS_INODE_LOCK_MAX_PERCPU

#define ODINFS_ENABLE_RANGE_LOCK_KMEM_CACHE 1

#define ODINFS_WRITE_WAIT_THRESHOLD 2097152L
#define ODINFS_READ_WAIT_THRESHOLD 2097152L

#define ODINFS_DELE_THREAD_BIND_TO_NUMA 0

#define ODINFS_DELE_THREAD_SLEEP 1

#endif /* __ODINFS_CONFIG_H_ */
