#ifndef __NOVA_CONFIG_H_
#define __NOVA_CONFIG_H_

#define NOVA_DELEGATION_ENABLE 1

#define NOVA_MAX_SOCKET 8
#define NOVA_MAX_AGENT_PER_SOCKET 28
#define NOVA_MAX_AGENT (NOVA_MAX_SOCKET * NOVA_MAX_AGENT_PER_SOCKET)

#define NOVA_AGENT_TASK_MAX_SIZE (8 + 1)

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when agents are serving
 * requests. The 3000 value is set with 32KB strip size where memcpy 32KB
 * takes around 70000 cycles. So (2.2*10^9) / 70000 = 3000
 */
#define NOVA_AGENT_REQUEST_CHECK_COUNT 3000

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when agents are spinning
 * on the ring buffer
 *
 * This assumes that one ring buffer acquire operation takes 100 cycles
 */

#define NOVA_AGENT_RING_BUFFER_CHECK_COUNT 220000

/*
 * Do cond_schuled()/kthread_should_stop() every 100ms when the application
 * threads are sending requests to the ring buffer
 *
 * This assumes that copying to the ring buffer takes around 100000 to complete
 *
 * The current ring buffer performance is unreasonable with 210+ application
 * threads, need to revise.
 */

#define NOVA_APP_RING_BUFFER_CHECK_COUNT 220

/*
 * Do cond_schuled() every 100ms when the application thread is waiting for
 * the delegation request to complete.
 */
#define NOVA_APP_CHECK_COUNT 220000000

#define NOVA_SOLROS_RING_BUFFER 1

/* write delegation limits: 256 */
#define NOVA_WRITE_DELEGATION_LIMIT 256

/* read delegation limits: 32K */
#define NOVA_READ_DELEGATION_LIMIT (32 * 1024)

/* Number of default delegation threads per socket */
#define NOVA_DEF_DELE_THREADS_PER_SOCKET 1

/* When set, use nt store to write to memory */
#define NOVA_NT_STORE 0

/* 2MB */
#define NOVA_RING_SIZE (2 * 1024 * 1024)

/*
 * Add this config to eliminate the effects of journaling when
 * study performance
 */
#define NOVA_ENABLE_JOURNAL 1

/* stock inode lock in the linux kernel */
#define NOVA_INODE_LOCK_STOCK 1
/* PCPU_RWSEM from the max paper */
#define NOVA_INODE_LOCK_MAX_PERCPU 2
/* stock per_cpu_rwsem in the  linux kernel */
#define NOVA_INODE_LOCK_PERCPU 3

#define NOVA_INODE_LOCK NOVA_INODE_LOCK_MAX_PERCPU

#define NOVA_WRITE_WAIT_THRESHOLD 2097152L
#define NOVA_READ_WAIT_THRESHOLD 2097152L

#define NOVA_DELE_THREAD_BIND_TO_NUMA 0

#define NOVA_DELE_THREAD_SLEEP 1

#endif /* __NOVA_CONFIG_H_ */
