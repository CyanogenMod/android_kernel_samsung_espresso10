/* arch/arm/mach-omap2/sec_gaf.c
 *
 * Copyright (C) 2011 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <asm/pgtable.h>

#include "sec_gaf.h"

/**{{ Add GAForensicINFO-1/2
 * Add GAForensic information
 */
static struct GAForensicINFO {
	unsigned short ver;
	unsigned int size;
	unsigned short task_struct_struct_state;
	unsigned short task_struct_struct_comm;
	unsigned short task_struct_struct_tasks;
	unsigned short task_struct_struct_pid;
	unsigned short task_struct_struct_stack;
	unsigned short task_struct_struct_mm;
	unsigned short mm_struct_struct_start_data;
	unsigned short mm_struct_struct_end_data;
	unsigned short mm_struct_struct_start_brk;
	unsigned short mm_struct_struct_brk;
	unsigned short mm_struct_struct_start_stack;
	unsigned short mm_struct_struct_arg_start;
	unsigned short mm_struct_struct_arg_end;
	unsigned short mm_struct_struct_pgd;
	unsigned short mm_struct_struct_mmap;
	unsigned short vm_area_struct_struct_vm_start;
	unsigned short vm_area_struct_struct_vm_end;
	unsigned short vm_area_struct_struct_vm_next;
	unsigned short vm_area_struct_struct_vm_file;
	unsigned short thread_info_struct_cpu_context;
	unsigned short cpu_context_save_struct_sp;
	unsigned short file_struct_f_path;
	unsigned short path_struct_mnt;
	unsigned short path_struct_dentry;
	unsigned short dentry_struct_d_parent;
	unsigned short dentry_struct_d_name;
	unsigned short qstr_struct_name;
	unsigned short vfsmount_struct_mnt_mountpoint;
	unsigned short vfsmount_struct_mnt_root;
	unsigned short vfsmount_struct_mnt_parent;
	unsigned int pgdir_shift;
	unsigned int ptrs_per_pte;
	unsigned int phys_offset;
	unsigned int page_offset;
	unsigned int page_shift;
	unsigned int page_size;
	unsigned short task_struct_struct_thread_group;
	unsigned short task_struct_struct_utime;
	unsigned short task_struct_struct_stime;
	unsigned short list_head_struct_next;
	unsigned short list_head_struct_prev;
	unsigned short rq_struct_curr;

	unsigned short thread_info_struct_cpu;

	unsigned short task_struct_struct_prio;
	unsigned short task_struct_struct_static_prio;
	unsigned short task_struct_struct_normal_prio;
	unsigned short task_struct_struct_rt_priority;

	unsigned short task_struct_struct_se;

	unsigned short sched_entity_struct_exec_start;
	unsigned short sched_entity_struct_sum_exec_runtime;
	unsigned short sched_entity_struct_prev_sum_exec_runtime;

	unsigned short task_struct_struct_sched_info;

	unsigned short sched_info_struct_pcount;
	unsigned short sched_info_struct_run_delay;
	unsigned short sched_info_struct_last_arrival;
	unsigned short sched_info_struct_last_queued;

	unsigned short task_struct_struct_blocked_on;

	unsigned short mutex_waiter_struct_list;
	unsigned short mutex_waiter_struct_task;

	unsigned short sched_entity_struct_cfs_rq_struct;
	unsigned short cfs_rq_struct_rq_struct;
	unsigned short gaf_fp;
	unsigned short GAFINFOCheckSum;
} GAFINFO = {
	.ver = 0x0300,
	.size = sizeof(GAFINFO),
	.task_struct_struct_state = offsetof(struct task_struct, state),
	.task_struct_struct_comm = offsetof(struct task_struct, comm),
	.task_struct_struct_tasks = offsetof(struct task_struct, tasks),
	.task_struct_struct_pid = offsetof(struct task_struct, pid),
	.task_struct_struct_stack = offsetof(struct task_struct, stack),
	.task_struct_struct_mm = offsetof(struct task_struct, mm),
	.mm_struct_struct_start_data = offsetof(struct mm_struct, start_data),
	.mm_struct_struct_end_data = offsetof(struct mm_struct, end_data),
	.mm_struct_struct_start_brk = offsetof(struct mm_struct, start_brk),
	.mm_struct_struct_brk = offsetof(struct mm_struct, brk),
	.mm_struct_struct_start_stack = offsetof(struct mm_struct, start_stack),
	.mm_struct_struct_arg_start = offsetof(struct mm_struct, arg_start),
	.mm_struct_struct_arg_end = offsetof(struct mm_struct, arg_end),
	.mm_struct_struct_pgd = offsetof(struct mm_struct, pgd),
	.mm_struct_struct_mmap = offsetof(struct mm_struct, mmap),
	.vm_area_struct_struct_vm_start =
		offsetof(struct vm_area_struct, vm_start),
	.vm_area_struct_struct_vm_end = offsetof(struct vm_area_struct, vm_end),
	.vm_area_struct_struct_vm_next =
		offsetof(struct vm_area_struct, vm_next),
	.vm_area_struct_struct_vm_file =
		offsetof(struct vm_area_struct, vm_file),
	.thread_info_struct_cpu_context =
		offsetof(struct thread_info, cpu_context),
	.cpu_context_save_struct_sp = offsetof(struct cpu_context_save, sp),
	.file_struct_f_path = offsetof(struct file, f_path),
	.path_struct_mnt = offsetof(struct path, mnt),
	.path_struct_dentry = offsetof(struct path, dentry),
	.dentry_struct_d_parent = offsetof(struct dentry, d_parent),
	.dentry_struct_d_name = offsetof(struct dentry, d_name),
	.qstr_struct_name = offsetof(struct qstr, name),
	.vfsmount_struct_mnt_mountpoint =
		offsetof(struct vfsmount, mnt_mountpoint),
	.vfsmount_struct_mnt_root = offsetof(struct vfsmount, mnt_root),
	.vfsmount_struct_mnt_parent = offsetof(struct vfsmount, mnt_parent),
	.pgdir_shift = PGDIR_SHIFT,
	.ptrs_per_pte = PTRS_PER_PTE,
	/* .phys_offset = PHYS_OFFSET, */
	.page_offset = PAGE_OFFSET,
	.page_shift = PAGE_SHIFT,
	.page_size = PAGE_SIZE,
	.task_struct_struct_thread_group =
		offsetof(struct task_struct, thread_group),
	.task_struct_struct_utime = offsetof(struct task_struct, utime),
	.task_struct_struct_stime = offsetof(struct task_struct, stime),
	.list_head_struct_next = offsetof(struct list_head, next),
	.list_head_struct_prev = offsetof(struct list_head, prev),

	.rq_struct_curr = 0,

	.thread_info_struct_cpu = offsetof(struct thread_info, cpu),

	.task_struct_struct_prio = offsetof(struct task_struct, prio),
	.task_struct_struct_static_prio =
		offsetof(struct task_struct, static_prio),
	.task_struct_struct_normal_prio =
		offsetof(struct task_struct, normal_prio),
	.task_struct_struct_rt_priority =
		offsetof(struct task_struct, rt_priority),

	.task_struct_struct_se = offsetof(struct task_struct, se),

	.sched_entity_struct_exec_start =
		offsetof(struct sched_entity, exec_start),
	.sched_entity_struct_sum_exec_runtime =
		offsetof(struct sched_entity, sum_exec_runtime),
	.sched_entity_struct_prev_sum_exec_runtime =
		offsetof(struct sched_entity, prev_sum_exec_runtime),

#if defined(CONFIG_SCHEDSTATS) || defined(CONFIG_TASK_DELAY_ACCT)
	.task_struct_struct_sched_info =
		offsetof(struct task_struct, sched_info),
	.sched_info_struct_pcount = offsetof(struct sched_info, pcount),
	.sched_info_struct_run_delay = offsetof(struct sched_info, run_delay),
	.sched_info_struct_last_arrival =
		offsetof(struct sched_info, last_arrival),
	.sched_info_struct_last_queued =
		offsetof(struct sched_info, last_queued),
#else
	.task_struct_struct_sched_info = 0x1223,
	.sched_info_struct_pcount = 0x1224,
	.sched_info_struct_run_delay = 0x1225,
	.sched_info_struct_last_arrival = 0x1226,
	.sched_info_struct_last_queued = 0x1227,
#endif

#ifdef CONFIG_DEBUG_MUTEXES
	.task_struct_struct_blocked_on =
		offsetof(struct task_struct, blocked_on),
	.mutex_waiter_struct_list = offsetof(struct mutex_waiter, list),
	.mutex_waiter_struct_task = offsetof(struct mutex_waiter, task),
#else
	.task_struct_struct_blocked_on = 0x1228,
	.mutex_waiter_struct_list = 0x1229,
	.mutex_waiter_struct_task = 0x122a,
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	.sched_entity_struct_cfs_rq_struct =
		offsetof(struct sched_entity, cfs_rq),
#else
	.sched_entity_struct_cfs_rq_struct = 0x1223,
#endif

	.cfs_rq_struct_rq_struct = 0,

#ifdef CONFIG_FRAME_POINTER
	.gaf_fp = 1,
#else
	.gaf_fp = 0,
#endif

	.GAFINFOCheckSum = 0
};

/**}} Add GAForensicINFO-1/2 */

void __init sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset)
{
	unsigned short *checksum = &(GAFINFO.GAFINFOCheckSum);
	unsigned char *memory = (unsigned char *)&GAFINFO;
	unsigned char address;

	/**{{ Add GAForensicINFO-2/2
	 *  Add GAForensic init for preventing symbol removal for optimization.
	 */
	GAFINFO.phys_offset = PHYS_OFFSET;
	GAFINFO.rq_struct_curr = curr_offset;

#ifdef CONFIG_FAIR_GROUP_SCHED
	GAFINFO.cfs_rq_struct_rq_struct = rq_offset;
#else
	GAFINFO.cfs_rq_struct_rq_struct = 0x1224;
#endif

	for (*checksum = 0, address = 0;
	     address < (sizeof(GAFINFO) - sizeof(GAFINFO.GAFINFOCheckSum));
	     address++) {
		if ((*checksum) & 0x8000)
			(*checksum) =
			    (((*checksum) << 1) | 1) ^ memory[address];
		else
			(*checksum) = ((*checksum) << 1) ^ memory[address];
	}
	/**}} Add GAForensicINFO-2/2 */
}

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

static void __sec_gaf_dump_one_task_info(struct task_struct *tsk, bool isMain)
{
	char stat_array[3] = { 'R', 'S', 'D' };
	char stat_ch;
	char *pThInf = tsk->stack;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];
	int permitted;
	struct mm_struct *mm;

	permitted = ptrace_may_access(tsk, PTRACE_MODE_READ);
	mm = get_task_mm(tsk);
	if (mm)
		if (permitted)
			pc = KSTK_EIP(tsk);

	wchan = get_wchan(tsk);

	if (lookup_symbol_name(wchan, symname) < 0) {
		if (!ptrace_may_access(tsk, PTRACE_MODE_READ))
			sprintf(symname, "_____");
		else
			sprintf(symname, "%lu", wchan);
	}

	stat_ch = tsk->state <= TASK_UNINTERRUPTIBLE ?
	    stat_array[tsk->state] : '?';

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  "
		"%08x %08x  %08x %c %16s [%s]\n",
		tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
		tsk->se.exec_start, stat_ch, (int)(tsk->state),
		*(int *)(pThInf + GAFINFO.thread_info_struct_cpu), (int)wchan,
		(int)pc, (int)tsk, isMain ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_UNINTERRUPTIBLE)
		pr_info("block_start: %16lld, block_max: %16lld\n",
			tsk->se.statistics.block_start,
			tsk->se.statistics.block_max);


	if (tsk->state == TASK_RUNNING ||
	    tsk->state == TASK_UNINTERRUPTIBLE || tsk->mm == NULL)
		show_stack(tsk, NULL);
}

void sec_gaf_dump_all_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n", current->pid, current->comm);
	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");
	pr_info("     pid    uTime    sTime         exec(ns) stat cpu     "
		"wchan  user_pc  ask_struct             comm sym_wchan\n");
	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		__sec_gaf_dump_one_task_info(curr_tsk, true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = container_of(curr_tsk->thread_group.next,
						struct task_struct,
						thread_group);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					__sec_gaf_dump_one_task_info(curr_thr,
								     false);
					curr_thr = container_of(
						curr_thr->thread_group.
						next, struct task_struct,
						thread_group);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
				struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}

	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");
}

void sec_gaf_dump_cpu_stat(void)
{
	int i, j;
	unsigned long jif;
	cputime64_t user, nice, system, idle, iowait, irq, softirq, steal;
	cputime64_t guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = { 0 };
	struct timespec boottime;
	unsigned int per_irq_sum;
	char *softirq_to_name[NR_SOFTIRQS] = {
		"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "BLOCK_IOPOLL",
		"TASKLET", "SCHED", "HRTIMER", "RCU"
	};
	unsigned int softirq_stat;

	user = nice = system = idle = iowait =
		irq = softirq = steal = cputime64_zero;
	guest = guest_nice = cputime64_zero;

	getboottime(&boottime);
	jif = boottime.tv_sec;
	for_each_possible_cpu(i) {
		user = cputime64_add(user, kstat_cpu(i).cpustat.user);
		nice = cputime64_add(nice, kstat_cpu(i).cpustat.nice);
		system = cputime64_add(system, kstat_cpu(i).cpustat.system);
		idle = cputime64_add(idle, kstat_cpu(i).cpustat.idle);
		idle = cputime64_add(idle, arch_idle_time(i));
		iowait = cputime64_add(iowait, kstat_cpu(i).cpustat.iowait);
		irq = cputime64_add(irq, kstat_cpu(i).cpustat.irq);
		softirq = cputime64_add(softirq, kstat_cpu(i).cpustat.softirq);

		for_each_irq_nr(j)
			sum += kstat_irqs_cpu(j, i);

		sum += arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			softirq_stat = kstat_softirqs_cpu(j, i);
			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();
	pr_info("");
	pr_info(" cpu    user:%-8llu nice:%-4llu system:%-4llu"
		"idle:%-8llu iowait:%-4llu irq:%-8llu"
		"softirq:%llu %llu %llu %llu\n",
		(unsigned long long)cputime64_to_clock_t(user),
		(unsigned long long)cputime64_to_clock_t(nice),
		(unsigned long long)cputime64_to_clock_t(system),
		(unsigned long long)cputime64_to_clock_t(idle),
		(unsigned long long)cputime64_to_clock_t(iowait),
		(unsigned long long)cputime64_to_clock_t(irq),
		(unsigned long long)cputime64_to_clock_t(softirq),
		(unsigned long long)0,	/* cputime64_to_clock_t(steal), */
		(unsigned long long)0,	/* cputime64_to_clock_t(guest), */
		(unsigned long long)0);	/* cputime64_to_clock_t(guest_nice)); */
	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");

	for_each_online_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kstat_cpu(i).cpustat.user;
		nice = kstat_cpu(i).cpustat.nice;
		system = kstat_cpu(i).cpustat.system;
		idle = kstat_cpu(i).cpustat.idle;
		idle = cputime64_add(idle, arch_idle_time(i));
		iowait = kstat_cpu(i).cpustat.iowait;
		irq = kstat_cpu(i).cpustat.irq;
		softirq = kstat_cpu(i).cpustat.softirq;
		/* steal = kstat_cpu(i).cpustat.steal; */
		/* guest = kstat_cpu(i).cpustat.guest; */
		/* guest_nice = kstat_cpu(i).cpustat.guest_nice; */
		pr_info(" cpu %2d user:%-8llu nice:%-4llu system:%-4llu"
			"idle:%-8llu iowait:%-4llu irq:%-8llu"
			"softirq:%llu %llu %llu %llu\n", i,
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)0,
			(unsigned long long)0,
			(unsigned long long)0);
	}

	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");
	pr_info("\n");
	pr_info(" irq       : %8llu", (unsigned long long)sum);
	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");

	/* sum again ? it could be updated? */
	for_each_irq_nr(j) {
		per_irq_sum = 0;
		for_each_possible_cpu(i)
			per_irq_sum += kstat_irqs_cpu(j, i);

		if (per_irq_sum) {
			pr_info(" irq-%-4d  : %8u %s\n", j, per_irq_sum,
				irq_to_desc(j)->action ?
					(irq_to_desc(j)->action->name ?
						: "???")
					: "???");
		}
	}

	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");
	pr_info("\n");
	pr_info(" softirq   : %8llu", (unsigned long long)sum_softirq);
	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");

	for (i = 0; i < NR_SOFTIRQS; i++)
		if (per_softirq_sums[i])
			pr_info(" softirq-%d : %8u %s\n", i,
				per_softirq_sums[i], softirq_to_name[i]);

	pr_info(" ------------------------------------------------------"
		"-------------------------------------------------------\n");
}
