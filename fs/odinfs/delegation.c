#include <linux/percpu.h>
#include <linux/uaccess.h>

#include "agent.h"
#include "delegation.h"
#include "odinfs_config.h"
#include "odinfs_stats.h"
#include "odinfs_utils.h"
#include "ring.h"

struct odinfs_notifyer_array {};

static inline int odinfs_choose_rings(void)
{
	return xor_random() % odinfs_num_of_rings_per_socket;
}

/* Per-client CPU variable */
/* Here we assume those values are initialized to 0 */
DEFINE_PER_CPU(struct odinfs_notifyer_array, odinfs_completed_cnt);

unsigned int odinfs_do_read_delegation(
	struct odinfs_sb_info *sbi, struct mm_struct *mm, unsigned long uaddr,
	unsigned long kaddr, unsigned long bytes, int zero,
	long *odinfs_issued_cnt, struct odinfs_notifyer *odinfs_completed_cnt,
	int wait_hint)
{
	int socket;
	unsigned int ret = 0;
	struct odinfs_delegation_request request;
	unsigned long i = 0, uaddr_end = 0;
	int block;
	int thread;
	ODINFS_DEFINE_TIMING_VAR(prefault_time);
	ODINFS_DEFINE_TIMING_VAR(send_request_time);

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

	ODINFS_START_TIMING(pre_fault_r_t, prefault_time);
	uaddr_end = ODINFS_ROUNDUP_PAGE(uaddr + bytes - 1);
	for (i = uaddr; i < uaddr_end; i += PAGE_SIZE) {
		unsigned long target_addr = i;

		/*
     * Do not access an address that is out of the buffer provided by the user
     */

		if (i > uaddr + bytes - 1)
			target_addr = uaddr + bytes - 1;

		odinfs_dbg_delegation(
			"uaddr: %lx, bytes: %ld, target_addr: %lx\n", uaddr,
			bytes, target_addr);

		ret = __clear_user((void *)target_addr, 1);

		if (ret != 0) {
			ODINFS_END_TIMING(pre_fault_r_t, prefault_time);
			goto out;
		}
	}
	ODINFS_END_TIMING(pre_fault_r_t, prefault_time);
	/*
   * We have ensured that [kaddr, kaddr + bytes - 1) falls in the same
   * kernel page.
   */

	/* which socket to delegate */

	block = odinfs_get_block_from_addr(sbi, (void *)kaddr);
	socket = odinfs_block_to_socket(sbi, block);

	/* inc issued cnt */
	odinfs_issued_cnt[socket]++;

	request.type = ODINFS_REQUEST_READ;
	request.mm = mm;
	request.uaddr = uaddr;
	request.kaddr = kaddr;
	request.bytes = bytes;
	request.zero = zero;

	/* Let the access threads increase the complete cnt */
	request.notify_cnt = &(odinfs_completed_cnt[socket].cnt);
	request.wait_hint = wait_hint;

	ODINFS_START_TIMING(send_request_r_t, send_request_time);
	do {
		thread = odinfs_choose_rings();
		ret = odinfs_send_request(odinfs_ring_buffer[socket][thread],
					  &request);
	} while (ret == -EAGAIN);

	wake_up_interruptible(&delegation_queue[socket][thread]);

	ODINFS_END_TIMING(send_request_r_t, send_request_time);

out:
	return ret;
}

/* make this a global variable so that the compiler will not optimize it */
int odinfs_no_optimize;

unsigned int odinfs_do_write_delegation(
	struct odinfs_sb_info *sbi, struct mm_struct *mm, unsigned long uaddr,
	unsigned long kaddr, unsigned long bytes, int zero, int flush_cache,
	int sfence, long *odinfs_issued_cnt,
	struct odinfs_notifyer *odinfs_completed_cnt, int wait_hint)
{
	int socket;
	struct odinfs_delegation_request request;
	int ret = 0;
	unsigned long i = 0, uaddr_end = 0;
	int block;
	int thread;

	ODINFS_DEFINE_TIMING_VAR(prefault_time);
	ODINFS_DEFINE_TIMING_VAR(send_request_time);
	ODINFS_DEFINE_TIMING_VAR(ring_buffer_enque_time);

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
		ODINFS_START_TIMING(pre_fault_w_t, prefault_time);
		uaddr_end = ODINFS_ROUNDUP_PAGE(uaddr + bytes - 1);
		for (i = uaddr; i < uaddr_end; i += PAGE_SIZE) {
			unsigned long target_addr = i;

			/*
       * Do not access an address that is out of the buffer provided by the user
       */

			if (i > uaddr + bytes - 1)
				target_addr = uaddr + bytes - 1;

			odinfs_dbg_delegation(
				"uaddr: %lx, bytes: %ld, target_addr: %lx\n",
				uaddr, bytes, target_addr);

			ret = copy_from_user(&odinfs_no_optimize,
					     (void *)target_addr, 1);

			if (ret != 0) {
				ODINFS_END_TIMING(pre_fault_w_t, prefault_time);
				goto out;
			}
		}
		ODINFS_END_TIMING(pre_fault_w_t, prefault_time);
	}

	/*
   * We have ensured that [kaddr, kaddr + bytes - 1) falls in the same
   * kernel page.
   */

	/* which socket to delegate */
	block = odinfs_get_block_from_addr(sbi, (void *)kaddr);
	socket = odinfs_block_to_socket(sbi, block);

	/* inc issued cnt */
	odinfs_issued_cnt[socket]++;

	request.type = ODINFS_REQUEST_WRITE;
	request.mm = mm;
	request.uaddr = uaddr;
	request.kaddr = kaddr;
	request.bytes = bytes;
	request.zero = zero;
	request.flush_cache = flush_cache;

	/* Let the access threads increase the complete cnt */
	request.notify_cnt = &(odinfs_completed_cnt[socket].cnt);
	request.wait_hint = wait_hint;

	ODINFS_START_TIMING(send_request_w_t, send_request_time);
	do {
		thread = odinfs_choose_rings();
		ODINFS_START_TIMING(ring_buffer_enque_w_t,
				    ring_buffer_enque_time);
		ret = odinfs_send_request(odinfs_ring_buffer[socket][thread],
					  &request);
		ODINFS_END_TIMING(ring_buffer_enque_w_t,
				  ring_buffer_enque_time);
	} while (ret == -EAGAIN);

	wake_up_interruptible(&delegation_queue[socket][thread]);

	ODINFS_END_TIMING(send_request_w_t, send_request_time);

out:
	return ret;
}

void odinfs_complete_delegation(long *odinfs_issued_cnt,
				struct odinfs_notifyer *odinfs_completed_cnt)
{
	int i = 0;

	for (i = 0; i < ODINFS_MAX_SOCKET; i++) {
		long issued = odinfs_issued_cnt[i];
		unsigned long cond_cnt = 0;

		/* TODO: Some kind of backoff is needed here? */
		while (issued != atomic_read(&(odinfs_completed_cnt[i].cnt))) {
			cond_cnt++;

			if (cond_cnt >= ODINFS_APP_CHECK_COUNT) {
				cond_cnt = 0;
				if (need_resched())
					cond_resched();
			}
		}
	}
}
