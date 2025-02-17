#include <linux/io.h>
#include <linux/kthread.h>

#include "agent.h"
#include "nova.h"
#include "nova_config.h"
#include "ring.h"

/*
 * TODO: memcpy is likely the bottleneck, so that the address translation part
 * should probably be moved to the main thread
 */

struct nova_agent_args {
	nova_ring_buffer_t *ring;
	int idx;
	int socket;
	int thread;
};

static struct task_struct *nova_agent_tasks[NOVA_MAX_AGENT];
static struct nova_agent_args nova_agent_args[NOVA_MAX_AGENT];

static int nova_num_of_agents = 0;

int nova_dele_thrds = NOVA_DEF_DELE_THREADS_PER_SOCKET;
int cpus_per_socket;

wait_queue_head_t delegation_queue[NOVA_MAX_SOCKET][NOVA_MAX_AGENT_PER_SOCKET];

/*
 * Very surprised to find that there is no such function in the kernel and I
 * need to write this function myself.
 */
static unsigned long user_virt_addr_to_phy_addr(struct mm_struct *mm,
						unsigned long uaddr)
{
	/* Mostly copied from mm_find_pmd and slow_virt_to_phys */
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pmd_t pmde;
	pte_t *pte;
	unsigned long phys_page_addr, offset;

	pgd = pgd_offset(mm, uaddr);
	if (!pgd_present(*pgd))
		goto out;

	p4d = p4d_offset(pgd, uaddr);
	if (!p4d_present(*p4d))
		goto out;

	pud = pud_offset(p4d, uaddr);
	if (!pud_present(*pud))
		goto out;

	if (pud_large(*pud)) {
		phys_page_addr = (phys_addr_t)pud_pfn(*pud) << PAGE_SHIFT;
		offset = uaddr & ~PUD_MASK;

		/*
     * This part has not been fully debugged and might be the issue,
     * do a printk here for the moment.
     */
		nova_warn("pud_large!\n");

		return (phys_page_addr | offset);
	}

	pmd = pmd_offset(pud, uaddr);

	pmde = *pmd;
	barrier();

	if (!pmd_present(pmde))
		goto out;

	if (pmd_large(pmde)) {
		phys_page_addr = (phys_addr_t)pmd_pfn(pmde) << PAGE_SHIFT;
		offset = uaddr & ~PMD_MASK;

		/*
     * This part has not been fully debugged and might be the issue,
     * do a printk here for the moment.
     */

		nova_warn("pmd_large!\n");

		return (phys_page_addr | offset);
	}

	/* weird, why there is a _kernel suffix */
	pte = pte_offset_kernel(pmd, uaddr);
	if (!pte_present(*pte))
		goto out;

	phys_page_addr = (phys_addr_t)pte_pfn(*pte) << PAGE_SHIFT;
	offset = uaddr & ~PAGE_MASK;

	return (phys_page_addr | offset);

out:
	return 0;
}

static int create_agent_tasks(struct mm_struct *mm, unsigned long uaddr,
			      unsigned long bytes,
			      struct nova_agent_tasks *tasks)
{
	unsigned long i = 0;
	long left_bytes = bytes;
	int tasks_index = 0;

	i = uaddr;

	while (i < uaddr + bytes) {
		unsigned long phy_addr = user_virt_addr_to_phy_addr(mm, i);
		unsigned long kuaddr = 0, size = 0;

		if (phy_addr == 0) {
			/* This should not happen */
			goto out;
		}

		size = ROUNDUP_PAGE(i) - i;

		if (size > left_bytes)
			size = left_bytes;

		left_bytes -= size;

		if (left_bytes < 0) {
			/* This should not happen */
			nova_warn("left_bytes underflow!\n");
			goto out;
		}

		kuaddr = (unsigned long)phys_to_virt(phy_addr);

		if ((tasks_index == 0) ||
		    (kuaddr != tasks[tasks_index - 1].kuaddr +
				       tasks[tasks_index - 1].size)) {
			tasks[tasks_index].kuaddr = kuaddr;
			tasks[tasks_index].size = size;
			tasks_index++;

			if (tasks_index > NOVA_AGENT_TASK_MAX_SIZE) {
				/* This should not happen */
				nova_warn("tasks_index overflow!\n");
				goto out;
			}
		} else {
			tasks[tasks_index - 1].size += size;
		}

		i += size;
	}

	return tasks_index;

out:
	return -1;
}

static void do_read_request(struct mm_struct *mm, unsigned long kaddr,
			    unsigned long uaddr, unsigned long bytes, int zero,
			    atomic_t *notify_cnt)
{
	int i = 0, tasks_index = 0;

	struct nova_agent_tasks tasks[NOVA_AGENT_TASK_MAX_SIZE];

	INIT_TIMING(address_translation_time);
	INIT_TIMING(memcpy_time);

	NOVA_START_TIMING(agent_addr_trans_r_t, address_translation_time);
	tasks_index = create_agent_tasks(mm, uaddr, bytes, tasks);
	NOVA_END_TIMING(agent_addr_trans_r_t, address_translation_time);

	if (tasks_index <= 0)
		goto out;

	nova_dbg("kaddr: %lx, uaddr: %lx, bytes: %ld", kaddr, uaddr, bytes);

	NOVA_START_TIMING(agent_memcpy_r_t, memcpy_time);
	for (i = 0; i < tasks_index; i++) {
		if (zero) {
			memset((void *)tasks[i].kuaddr, 0, tasks[i].size);
		} else {
			nova_dbg("uaddr: %lx, size: %ld, kaddr: %lx\n",
				 tasks[i].kuaddr, tasks[i].size, kaddr);

			memcpy((void *)tasks[i].kuaddr, (void *)kaddr,
			       tasks[i].size);
			kaddr += tasks[i].size;
		}
	}
	NOVA_END_TIMING(agent_memcpy_r_t, memcpy_time);

out:
	atomic_inc(notify_cnt);
	return;
}

/*
 * There are different ways to flush the cache. Here we follow what
 * nova do that.
 *
 * Memset: nt
 * others: clwb
 *
 * TODO: Need to think of how to do sfence in this case
 */
static void do_write_request(struct mm_struct *mm, unsigned long kaddr,
			     unsigned long uaddr, unsigned long bytes, int zero,
			     int flush_cache, atomic_t *notify_cnt)
{
	int i = 0, tasks_index = 0;
	unsigned long orig_kaddr = kaddr;

	struct nova_agent_tasks tasks[NOVA_AGENT_TASK_MAX_SIZE];

	INIT_TIMING(address_translation_time);
	INIT_TIMING(memcpy_time);

	if (zero) {
		nova_dbg_verbose("%s: zero flush_cache:%d\n", __func__,
				 flush_cache);
		NOVA_START_TIMING(agent_memcpy_w_t, memcpy_time);
		if (flush_cache)
			memset_nt((void *)kaddr, 0, bytes);
		else
			memset((void *)kaddr, 0, bytes);

		NOVA_END_TIMING(agent_memcpy_w_t, memcpy_time);
		goto out;
	}

	NOVA_START_TIMING(agent_addr_trans_w_t, address_translation_time);
	tasks_index = create_agent_tasks(mm, uaddr, bytes, tasks);
	NOVA_END_TIMING(agent_addr_trans_w_t, address_translation_time);

	if (tasks_index <= 0)
		goto out;

	nova_dbg("kaddr: %lx, uaddr: %lx, bytes: %ld", kaddr, uaddr, bytes);

	NOVA_START_TIMING(agent_memcpy_w_t, memcpy_time);
	for (i = 0; i < tasks_index; i++) {
		nova_dbg("uaddr: %lx, size: %ld, kaddr: %lx\n", tasks[i].kuaddr,
			 tasks[i].size, kaddr);

#if NOVA_NT_STORE
		__copy_from_user_inatomic_nocache(
			(void *)kaddr, (void *)tasks[i].kuaddr, tasks[i].size);
#else
		memcpy((void *)kaddr, (void *)tasks[i].kuaddr, tasks[i].size);
#endif

		kaddr += tasks[i].size;
	}

#if !NOVA_NT_STORE
	if (flush_cache)
		nova_flush_buffer((void *)orig_kaddr, bytes, 0);
#endif

	NOVA_END_TIMING(agent_memcpy_w_t, memcpy_time);

out:
	atomic_inc(notify_cnt);
	return;
}

static int agent_func(void *arg)
{
	struct nova_delegation_request request;
	nova_ring_buffer_t *ring = ((struct nova_agent_args *)arg)->ring;
	int socket = ((struct nova_agent_args *)arg)->socket;
	int thread = ((struct nova_agent_args *)arg)->thread;
	int oldnice = task_nice(current);
	int wait_hint = 0;

	int err = 0;
	unsigned long cond_cnt = 0;

	/* decrease the kernel delegation thread nice value while handling request */
#if NOVA_DELE_THREAD_SLEEP
	sched_set_normal(current, MIN_NICE);
#endif

	while (1) {
		INIT_TIMING(recv_request_time);
		NOVA_START_TIMING(agent_receive_request_t, recv_request_time);

#if NOVA_DELE_THREAD_SLEEP
		/* Request side says the request takes a longer period to finish, so
		 * the optimized strategy is to directly go to sleep instead of spin */
		if (!wait_hint)
			goto spin_and_wait;

wait:
		wait_event_interruptible(delegation_queue[socket][thread],
					 nova_recv_request(ring, &request) !=
							 -EAGAIN ||
						 kthread_should_stop());

		if (need_resched())
			cond_resched();

		goto process_request;

spin_and_wait:
		if (need_resched()) {
			goto wait;
		} else {
			err = nova_recv_request(ring, &request);
			if (err == -EAGAIN)
				goto spin_and_wait;
		}
#else

try_again:
		err = nova_recv_request(ring, &request);

		if (err == -EAGAIN) {
			cond_cnt++;

			if (cond_cnt >= NOVA_AGENT_RING_BUFFER_CHECK_COUNT) {
				if (kthread_should_stop())
					break;

				if (need_resched())
					cond_resched();
				cond_cnt = 0;
			}

			goto try_again;
		}

		if (err)
			break;
#endif

		NOVA_END_TIMING(agent_receive_request_t, recv_request_time);

process_request:
		if (kthread_should_stop())
			break;

		wait_hint = request.wait_hint;

		if (request.type == NOVA_REQUEST_READ) {
			do_read_request(request.mm, request.kaddr,
					request.uaddr, request.bytes,
					request.zero, request.notify_cnt);
		} else if (request.type == NOVA_REQUEST_WRITE) {
			do_write_request(request.mm, request.kaddr,
					 request.uaddr, request.bytes,
					 request.zero, request.flush_cache,
					 request.notify_cnt);
		} else {
			nova_warn("Unknown request type: %d", request.type);
		}

		cond_cnt++;

		if (cond_cnt >= NOVA_AGENT_REQUEST_CHECK_COUNT) {
			if (kthread_should_stop())
				break;

			if (need_resched())
				cond_resched();

			cond_cnt = 0;
		}
	}

	/* set the kernel delegation thread nice value back */
#if NOVA_DELE_THREAD_SLEEP
	sched_set_normal(current, oldnice);
#endif

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return err;
}

int nova_init_agents(int cpus, int sockets)
{
	int i = 0, j = 0;
	int ret = 0;

	char name[255];
#if NOVA_DELE_THREAD_BIND_TO_NUMA
	struct cpumask *numa_cpumask;
#endif
	memset(nova_agent_tasks, 0,
	       sizeof(struct task_struct *) * NOVA_MAX_AGENT);

	cpus_per_socket = num_online_cpus() / num_online_nodes();
	nova_num_of_agents = sockets * nova_dele_thrds;

	for (i = 0; i < sockets; i++) {
		for (j = 0; j < nova_dele_thrds; j++) {
			/* Use the first few cpus of each socket */
			int target_cpu = i * cpus_per_socket + j;
			int index = i * nova_dele_thrds + j;
			struct task_struct *task;

			init_waitqueue_head(&delegation_queue[i][j]);

			nova_agent_args[index].ring = nova_ring_buffer[i][j];
			nova_agent_args[index].idx = index;
			nova_agent_args[index].socket = i;
			nova_agent_args[index].thread = j;

			sprintf(name, "nova_agent_%d_cpu_%d", index,
				target_cpu);

			task = kthread_create(agent_func,
					      &nova_agent_args[index], name);

			if (IS_ERR(task)) {
				ret = PTR_ERR(task);
				goto error;
			}

			nova_agent_tasks[index] = task;

#if NOVA_DELE_THREAD_BIND_TO_NUMA
			numa_cpumask = cpumask_of_node(i);
			kthread_bind_mask(nova_agent_tasks[index],
					  numa_cpumask);
#else
			kthread_bind(nova_agent_tasks[index], target_cpu);
#endif
			wake_up_process(nova_agent_tasks[index]);
		}
	}

	return 0;

error:
	nova_agents_fini();

	return -ENOMEM;
}

void nova_agents_fini(void)
{
	int i = 0;

	for (i = 0; i < nova_num_of_agents; i++) {
		if (nova_agent_tasks[i]) {
			int ret;
			if ((ret = kthread_stop(nova_agent_tasks[i])))
				nova_dbg(
					"kthread_stop task returned error %d\n",
					ret);
		}
	}
}
