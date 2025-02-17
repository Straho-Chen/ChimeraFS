#include <linux/percpu.h>
#include <linux/uaccess.h>

#include "nova.h"
#include "agent.h"
#include "ring.h"

struct nova_notifyer_array {};

static inline int nova_choose_rings(void)
{
	return xor_random() % nova_num_of_rings_per_socket;
}

/* Per-client CPU variable */
/* Here we assume those values are initialized to 0 */
DEFINE_PER_CPU(struct nova_notifyer_array, completed_cnt);

unsigned int nova_do_read_delegation(struct nova_sb_info *sbi,
				     struct mm_struct *mm, unsigned long uaddr,
				     unsigned long kaddr, unsigned long bytes,
				     int zero, long *issued_cnt,
				     struct nova_notifyer *completed_cnt,
				     int wait_hint)
{
	int socket;
	unsigned int ret = 0;
	struct nova_delegation_request request;
	unsigned long i = 0, uaddr_end = 0;
	int block;
	int thread;
	INIT_TIMING(prefault_time);
	INIT_TIMING(send_request_time);
	INIT_TIMING(ring_buffer_enque_time);

	/* TODO: Check the validity of the user-level address */

	/*
   * access the user address while still at the process's address space
   * to let the kernel handles various situations: e.g., page not mapped,
   * page swapped out.
   *
   * Surprisingly, the overhead of this part is significant. However,
   * currently it looks to me no point to optimize this part; The bottleneck
   * is in the delegation thread, not the main thread.
   */

	NOVA_START_TIMING(pre_fault_r_t, prefault_time);
	uaddr_end = ROUNDUP_PAGE(uaddr + bytes - 1);
	for (i = uaddr; i < uaddr_end; i += PAGE_SIZE) {
		unsigned long target_addr = i;

		/*
     * Do not access an address that is out of the buffer provided by the user
     */

		if (i > uaddr + bytes - 1)
			target_addr = uaddr + bytes - 1;

		nova_dbg("uaddr: %lx, bytes: %ld, target_addr: %lx\n", uaddr,
			 bytes, target_addr);

		ret = __clear_user((void *)target_addr, 1);

		if (ret != 0) {
			NOVA_END_TIMING(pre_fault_r_t, prefault_time);
			goto out;
		}
	}
	NOVA_END_TIMING(pre_fault_r_t, prefault_time);
	/*
   * We have ensured that [kaddr, kaddr + bytes - 1) falls in the same
   * kernel page.
   */

	/* which socket to delegate */

	block = nova_get_block_from_addr(sbi, (void *)kaddr);
	socket = nova_block_to_socket(sbi, block);

	/* inc issued cnt */
	issued_cnt[socket]++;

	request.type = NOVA_REQUEST_READ;
	request.mm = mm;
	request.uaddr = uaddr;
	request.kaddr = kaddr;
	request.bytes = bytes;
	request.zero = zero;

	/* Let the access threads increase the complete cnt */
	request.notify_cnt = &(completed_cnt[socket].cnt);
	request.wait_hint = wait_hint;

	NOVA_START_TIMING(send_request_r_t, send_request_time);
	do {
		thread = nova_choose_rings();
		NOVA_START_TIMING(ring_buffer_enque_r_t,
				  ring_buffer_enque_time);
		ret = nova_send_request(nova_ring_buffer[socket][thread],
					&request);
		NOVA_END_TIMING(ring_buffer_enque_r_t, ring_buffer_enque_time);
	} while (ret == -EAGAIN);

	wake_up_interruptible(&delegation_queue[socket][thread]);

	NOVA_END_TIMING(send_request_r_t, send_request_time);

out:
	return ret;
}

/* make this a global variable so that the compiler will not optimize it */
int nova_no_optimize;

unsigned int nova_do_write_delegation(struct nova_sb_info *sbi,
				      struct mm_struct *mm, unsigned long uaddr,
				      unsigned long kaddr, unsigned long bytes,
				      int zero, int flush_cache, int sfence,
				      long *issued_cnt,
				      struct nova_notifyer *completed_cnt,
				      int wait_hint)
{
	int socket;
	struct nova_delegation_request request;
	int ret = 0;
	unsigned long i = 0, uaddr_end = 0;
	int block;
	int thread;

	INIT_TIMING(prefault_time);
	INIT_TIMING(send_request_time);
	INIT_TIMING(ring_buffer_enque_time);

	/* TODO: Check the validity of the user-level address */

	/*
   * access the user address while still at the process's address space
   * to let the kernel handles various situations: e.g., page not mapped,
   * page swapped out.
   *
   * Surprisingly, the overhead of this part is significant. However,
   * currently it looks to me no point to optimize this part; The bottleneck
   * is in the delegation thread, not the main thread.
   */
	if (!zero) {
		NOVA_START_TIMING(pre_fault_w_t, prefault_time);
		uaddr_end = ROUNDUP_PAGE(uaddr + bytes - 1);
		for (i = uaddr; i < uaddr_end; i += PAGE_SIZE) {
			unsigned long target_addr = i;

			/*
       * Do not access an address that is out of the buffer provided by the user
       */

			if (i > uaddr + bytes - 1)
				target_addr = uaddr + bytes - 1;

			nova_dbg("uaddr: %lx, bytes: %ld, target_addr: %lx\n",
				 uaddr, bytes, target_addr);

			ret = copy_from_user(&nova_no_optimize,
					     (void *)target_addr, 1);

			if (ret != 0) {
				NOVA_END_TIMING(pre_fault_w_t, prefault_time);
				goto out;
			}
		}
		NOVA_END_TIMING(pre_fault_w_t, prefault_time);
	}

	/*
   * We have ensured that [kaddr, kaddr + bytes - 1) falls in the same
   * kernel page.
   */

	/* which socket to delegate */
	block = nova_get_block_from_addr(sbi, (void *)kaddr);
	socket = nova_block_to_socket(sbi, block);

	/* inc issued cnt */
	issued_cnt[socket]++;

	request.type = NOVA_REQUEST_WRITE;
	request.mm = mm;
	request.uaddr = uaddr;
	request.kaddr = kaddr;
	request.bytes = bytes;
	request.zero = zero;
	request.flush_cache = flush_cache;

	/* Let the access threads increase the complete cnt */
	request.notify_cnt = &(completed_cnt[socket].cnt);
	request.wait_hint = wait_hint;

	NOVA_START_TIMING(send_request_w_t, send_request_time);
	do {
		thread = nova_choose_rings();
		NOVA_START_TIMING(ring_buffer_enque_w_t,
				  ring_buffer_enque_time);
		ret = nova_send_request(nova_ring_buffer[socket][thread],
					&request);
		NOVA_END_TIMING(ring_buffer_enque_w_t, ring_buffer_enque_time);
	} while (ret == -EAGAIN);

	wake_up_interruptible(&delegation_queue[socket][thread]);

	NOVA_END_TIMING(send_request_w_t, send_request_time);

out:
	return ret;
}

void nova_complete_delegation(long *issued_cnt,
			      struct nova_notifyer *completed_cnt)
{
	int i = 0;

	for (i = 0; i < NOVA_MAX_SOCKET; i++) {
		long issued = issued_cnt[i];
		unsigned long cond_cnt = 0;

		/* TODO: Some kind of backoff is needed here? */
		while (issued != atomic_read(&(completed_cnt[i].cnt))) {
			cond_cnt++;

			if (cond_cnt >= NOVA_APP_CHECK_COUNT) {
				cond_cnt = 0;
				if (need_resched())
					cond_resched();
			}
		}
	}
}