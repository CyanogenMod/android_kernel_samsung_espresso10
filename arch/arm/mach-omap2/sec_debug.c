/* arch/arm/mach-omap2/sec_debug.c
 *
 * Copyright (C) 2010-2011 Samsung Electronics Co, Ltd.
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

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/keyboard.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/sysrq.h>
#include <linux/uaccess.h>

#include <mach/system.h>

#include <asm/cacheflush.h>

#include "sec_debug.h"
#include "sec_gaf.h"

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT = 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC = 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD = 0x00000022,
	UPLOAD_CAUSE_CP_ERROR_FATAL = 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT = 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED = 0x000000DD,
};

struct sec_debug_mmu_reg_t {
	unsigned int SCTLR;
	unsigned int TTBR0;
	unsigned int TTBR1;
	unsigned int TTBCR;
	unsigned int DACR;
	unsigned int DFSR;
	unsigned int DFAR;
	unsigned int IFSR;
	unsigned int IFAR;
	unsigned int DAFSR;
	unsigned int IAFSR;
	unsigned int PMRRR;
	unsigned int NMRRR;
	unsigned int FCSEPID;
	unsigned int CONTEXT;
	unsigned int URWTPID;
	unsigned int UROTPID;
	unsigned int POTPIDR;
};

/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;

	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;

	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;

	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;

	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;

	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;

	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;

	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;

	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;

};

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };

module_param_named(enable, sec_debug_level.en.kernel_fault, ushort,
		S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort,
		S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(level, sec_debug_level.uint_val, uint,
		S_IRUGO | S_IWUSR | S_IWGRP);

static int __init sec_debug_parse_enable(char *str)
{
	if (kstrtou16(str, 0, &sec_debug_level.en.kernel_fault))
		sec_debug_level.en.kernel_fault = 1;
	return 0;
}
early_param("sec_debug.enable", sec_debug_parse_enable);

static int __init sec_debug_parse_enable_user(char *str)
{
	if (kstrtou16(str, 0, &sec_debug_level.en.user_fault))
		sec_debug_level.en.user_fault = 0;
	return 0;
}
early_param("sec_debug.enable_user", sec_debug_parse_enable_user);

static int __init sec_debug_parse_level(char *str)
{
	if (kstrtou32(str, 0, &sec_debug_level.uint_val))
		sec_debug_level.uint_val = 1;
	return 0;
}
early_param("sec_debug.level", sec_debug_parse_level);

static const char * const gkernel_sec_build_info_date_time[] = {
	__DATE__,
	__TIME__
};

static char gkernel_sec_build_info[100];

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX		2048

struct sched_log {
	struct task_log {
		unsigned long long time;
		char comm[TASK_COMM_LEN];
		pid_t pid;
	} task[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	struct irq_log {
		unsigned long long time;
		int irq;
		void *fn;
		int en;
	} irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	struct work_log {
		unsigned long long time;
		struct worker *worker;
		struct work_struct *work;
		work_func_t f;
	} work[CONFIG_NR_CPUS][SCHED_LOG_MAX];
};

static struct sched_log sec_debug_log __cacheline_aligned;

static struct sched_log (*psec_debug_log) = (&sec_debug_log);

static atomic_t task_log_idx[NR_CPUS] = {
	ATOMIC_INIT(-1), ATOMIC_INIT(-1)
};
static atomic_t irq_log_idx[NR_CPUS] = {
	ATOMIC_INIT(-1), ATOMIC_INIT(-1)
};
static atomic_t work_log_idx[NR_CPUS] = {
	ATOMIC_INIT(-1), ATOMIC_INIT(-1)
};

static int checksum_sched_log(void)
{
	int sum = 0, i;

	for (i = 0; i < sizeof(sec_debug_log); i++)
		sum += *((char *)&sec_debug_log + i);

	return sum;
}
#else
static int checksum_sched_log(void)
{
	return 0;
}
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
#define SEMAPHORE_LOG_MAX		100

struct sem_debug {
	struct list_head list;
	struct semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	/* char comm[TASK_COMM_LEN]; */
};

enum {
	READ_SEM,
	WRITE_SEM,
};

static struct sem_debug sem_debug_free_head;
static struct sem_debug sem_debug_done_head;
static int sem_debug_free_head_cnt;
static int sem_debug_done_head_cnt;
static int sem_debug_init;
static spinlock_t sem_debug_lock;

/* rwsemaphore logging */
#define RWSEMAPHORE_LOG_MAX		100

struct rwsem_debug {
	struct list_head list;
	struct rw_semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	int direction;
	/* char comm[TASK_COMM_LEN]; */
};

static struct rwsem_debug rwsem_debug_free_head;
static struct rwsem_debug rwsem_debug_done_head;
static int rwsem_debug_free_head_cnt;
static int rwsem_debug_done_head_cnt;
static int rwsem_debug_init;
static spinlock_t rwsem_debug_lock;

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

/* core reg dump function*/
static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	   SVC registers along with cmn registers. */
	asm("str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
	    "mov r0, %0\n\t"		/* R0 will be alias for core_reg */
	    "str r1, [r0,#4]\n\t"	/* R1 */
	    "str r2, [r0,#8]\n\t"	/* R2 */
	    "str r3, [r0,#12]\n\t"	/* R3 */
	    "str r4, [r0,#16]\n\t"	/* R4 */
	    "str r5, [r0,#20]\n\t"	/* R5 */
	    "str r6, [r0,#24]\n\t"	/* R6 */
	    "str r7, [r0,#28]\n\t"	/* R7 */
	    "str r8, [r0,#32]\n\t"	/* R8 */
	    "str r9, [r0,#36]\n\t"	/* R9 */
	    "str r10, [r0,#40]\n\t"	/* R10 */
	    "str r11, [r0,#44]\n\t"	/* R11 */
	    "str r12, [r0,#48]\n\t"	/* R12 */
	    /* SVC */
	    "str r13, [r0,#52]\n\t"	/* R13_SVC */
	    "str r14, [r0,#56]\n\t"	/* R14_SVC */
	    "mrs r1, spsr\n\t"		/* SPSR_SVC */
	    "str r1, [r0,#60]\n\t"
	    /* PC and CPSR */
	    "sub r1, r15, #0x4\n\t"	/* PC */
	    "str r1, [r0,#64]\n\t"
	    "mrs r1, cpsr\n\t"		/* CPSR */
	    "str r1, [r0,#68]\n\t"
	    /* SYS/USR */
	    "mrs r1, cpsr\n\t"		/* switch to SYS mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1f\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#72]\n\t"	/* R13_USR */
	    "str r14, [r0,#76]\n\t"	/* R14_USR */
	    /* FIQ */
	    "mrs r1, cpsr\n\t"		/* switch to FIQ mode */
	    "and r1,r1,#0xFFFFFFE0\n\t"
	    "orr r1,r1,#0x11\n\t"
	    "msr cpsr,r1\n\t"
	    "str r8, [r0,#80]\n\t"	/* R8_FIQ */
	    "str r9, [r0,#84]\n\t"	/* R9_FIQ */
	    "str r10, [r0,#88]\n\t"	/* R10_FIQ */
	    "str r11, [r0,#92]\n\t"	/* R11_FIQ */
	    "str r12, [r0,#96]\n\t"	/* R12_FIQ */
	    "str r13, [r0,#100]\n\t"	/* R13_FIQ */
	    "str r14, [r0,#104]\n\t"	/* R14_FIQ */
	    "mrs r1, spsr\n\t"		/* SPSR_FIQ */
	    "str r1, [r0,#108]\n\t"
	    /* IRQ */
	    "mrs r1, cpsr\n\t"		/* switch to IRQ mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x12\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#112]\n\t"	/* R13_IRQ */
	    "str r14, [r0,#116]\n\t"	/* R14_IRQ */
	    "mrs r1, spsr\n\t"		/* SPSR_IRQ */
	    "str r1, [r0,#120]\n\t"
	    /* MON */
	    "mrs r1, cpsr\n\t"		/* switch to monitor mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x16\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#124]\n\t"	/* R13_MON */
	    "str r14, [r0,#128]\n\t"	/* R14_MON */
	    "mrs r1, spsr\n\t"		/* SPSR_MON */
	    "str r1, [r0,#132]\n\t"
	    /* ABT */
	    "mrs r1, cpsr\n\t"		/* switch to Abort mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x17\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#136]\n\t"	/* R13_ABT */
	    "str r14, [r0,#140]\n\t"	/* R14_ABT */
	    "mrs r1, spsr\n\t"		/* SPSR_ABT */
	    "str r1, [r0,#144]\n\t"
	    /* UND */
	    "mrs r1, cpsr\n\t"		/* switch to undef mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x1B\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#148]\n\t"	/* R13_UND */
	    "str r14, [r0,#152]\n\t"	/* R14_UND */
	    "mrs r1, spsr\n\t"		/* SPSR_UND */
	    "str r1, [r0,#156]\n\t"
	    /* restore to SVC mode */
	    "mrs r1, cpsr\n\t"		/* switch to SVC mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x13\n\t"
	    "msr cpsr,r1\n\t" :		/* output */
	    : "r"(core_reg)		/* input */
	    : "%r0", "%r1"		/* clobbered registers */
	);

	return;
}

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
}

#define __sec_debug_dump_1_reg(_container, _reg0)			\
do {									\
	pr_emerg(" %-8s: %08x\n", #_reg0, _container->_reg0);		\
} while (0)

#define __sec_debug_dump_2_reg(_container, _reg0, _reg1)		\
do {									\
	pr_emerg(" %-8s: %08x %-8s: %08x\n",				\
		#_reg0, _container->_reg0, #_reg1, _container->_reg1);	\
} while (0)

#define __sec_debug_dump_3_reg(_container, _reg0, _reg1, _reg2)		\
do {									\
	pr_emerg(" %-8s: %08x %-8s: %08x %-8s: %08x\n",			\
		#_reg0, _container->_reg0, #_reg1, _container->_reg1,	\
		#_reg2,	_container->_reg2);				\
} while (0)

#define __sec_debug_dump_4_reg(_container, _reg0, _reg1, _reg2, _reg3)	\
do {									\
	pr_emerg(" %-8s: %08x %-8s: %08x %-8s: %08x %-8s: %08x\n",	\
		#_reg0, _container->_reg0, #_reg1, _container->_reg1,	\
		#_reg2,	_container->_reg2, #_reg3, _container->_reg3);	\
} while (0)

static void sed_debug_dump_core_reg(struct sec_debug_core_t *core_reg)
{
	pr_emerg("Sec Core REGs:\n");
	pr_emerg(" - Common\n");
	__sec_debug_dump_4_reg(core_reg, r0, r1, r2, r3);
	__sec_debug_dump_4_reg(core_reg, r4, r5, r6, r7);
	__sec_debug_dump_4_reg(core_reg, r8, r9, r10, r11);
	__sec_debug_dump_1_reg(core_reg, r12);
	pr_emerg(" - SVC\n");
	__sec_debug_dump_3_reg(core_reg, r13_svc, r14_svc, spsr_svc);
	pr_emerg(" - PC & CPSR\n");
	__sec_debug_dump_2_reg(core_reg, pc, cpsr);
	pr_emerg(" - USR/SYS\n");
	__sec_debug_dump_2_reg(core_reg, r13_usr, r14_usr);
	pr_emerg(" - FIQ\n");
	__sec_debug_dump_4_reg(core_reg, r8_fiq, r9_fiq, r10_fiq, r11_fiq);
	__sec_debug_dump_4_reg(core_reg, r12_fiq, r13_fiq, r14_fiq, spsr_fiq);
	pr_emerg(" - IRQ\n");
	__sec_debug_dump_3_reg(core_reg, r13_irq, r14_irq, spsr_irq);
	pr_emerg(" - MON\n");
	__sec_debug_dump_3_reg(core_reg, r13_mon, r14_mon, spsr_mon);
	pr_emerg(" - ABT\n");
	__sec_debug_dump_3_reg(core_reg, r13_abt, r14_abt, spsr_abt);
	pr_emerg(" - UNDEF\n");
	__sec_debug_dump_3_reg(core_reg, r13_und, r14_und, spsr_und);
}

static void sec_debug_dump_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	pr_emerg("Sec MMU REGs:\n");
	__sec_debug_dump_3_reg(mmu_reg, SCTLR, TTBR0, TTBR1);
	__sec_debug_dump_3_reg(mmu_reg, DACR, DFSR, DFAR);
	__sec_debug_dump_4_reg(mmu_reg, IFSR, IFAR, DAFSR, IAFSR);
	__sec_debug_dump_4_reg(mmu_reg, PMRRR, NMRRR, FCSEPID, CONTEXT);
	__sec_debug_dump_3_reg(mmu_reg, URWTPID, UROTPID, POTPIDR);
}

static void sec_debug_save_context(void)
{
	unsigned long flags;

	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	sec_debug_dump_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sed_debug_dump_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));
	local_irq_restore(flags);
}

static void sec_debug_set_upload_magic(unsigned int magic)
{
	pr_debug("(%s) %x\n", __func__, magic);

#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4)
	omap_writel(magic, SEC_DEBUG_MAGIC_ADDR);
#elif defined(CONFIG_ARCH_S5PV310)
	*(unsigned int *)SEC_DEBUG_MAGIC_ADDR = magic;
#endif /* CONFIG_ARCH_* */

	flush_cache_all();
	outer_flush_all();
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	sec_debug_set_upload_magic(0x0);

	return 0;
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;

#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4)
	omap_writel(type, SEC_DEBUG_UPLOAD_CAUSE_ADDR);
#elif defined(CONFIG_ARCH_S5PV310)
	/* to check VDD_ALIVE / XnRESET issue */
	__raw_writel(type, S5P_INFORM3);
	__raw_writel(type, S5P_INFORM4);
	__raw_writel(type, S5P_INFORM6);
#endif /* CONFIG_ARCH_* */

	pr_debug("(%s) %x\n", __func__, type);
}

/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
static inline int sec_debug_dump_stack(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}

static inline void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) %s\n", __func__, gkernel_sec_build_info);
	pr_emerg("(%s) rebooting...\n", __func__);

	flush_cache_all();
	outer_flush_all();

#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4)
	arch_reset('L', "Lock-Up");
#elif defined(CONFIG_ARCH_S5PV310)
	arch_reset(0, 0);
#endif /* CONFIG_ARCH_* */

	while (1)
		;
}

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	local_irq_disable();

	sec_debug_set_upload_magic(SEC_DEBUG_MAGIC_DUMP);

	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		/* more information will be provided after "CP Crash" string */
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "HSIC Disconnected"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());

	sec_gaf_dump_all_task_info();
	sec_gaf_dump_cpu_stat();

	sec_debug_dump_stack();
	sec_debug_hw_reset();

	return 0;
}

static unsigned int crash_key_code[] = { KEY_VOLUMEDOWN, KEY_VOLUMEUP,
					 KEY_POWER };

static struct sec_crash_key dflt_crash_key = {
	.keycode	= crash_key_code,
	.size		= ARRAY_SIZE(crash_key_code),
	.timeout	= 1000,		/* 1 sec */
};

static struct sec_crash_key *__sec_crash_key = &dflt_crash_key;

static unsigned int sec_debug_unlock_crash_key(unsigned int value,
					       unsigned int *unlock)
{
	unsigned int i = __sec_crash_key->size - 2;	/* except last one */
	unsigned int ret = 0;

	do {
		if (value == __sec_crash_key->keycode[i]) {
			ret = 1;
			*unlock |= 1 << i;
		}
	} while (i-- != 0);

	return ret;
}

static void sec_debug_check_crash_key(unsigned int value, int down)
{
	static unsigned long unlock_jiffies;
	static unsigned long trigger_jiffies;
	static bool other_key_pressed;
	static unsigned int unlock;
	unsigned int timeout;

	if (!down)
		goto __clear_all;

	if (sec_debug_unlock_crash_key(value, &unlock)) {
		if (unlock == __sec_crash_key->unlock)
			unlock_jiffies = jiffies;
	} else if (value ==
		   __sec_crash_key->keycode[__sec_crash_key->size - 1]) {
		trigger_jiffies = jiffies;
	} else {
		other_key_pressed = true;
		goto __clear_timeout;
	}

	if (unlock_jiffies && trigger_jiffies && !other_key_pressed &&
	    time_after(trigger_jiffies, unlock_jiffies)) {
		timeout = jiffies_to_msecs(trigger_jiffies - unlock_jiffies);
		if (timeout < __sec_crash_key->timeout)
			panic("Crash Key");
		else
			goto __clear_all;
	}

	return;

__clear_all:
	other_key_pressed = false;
__clear_timeout:
	unlock_jiffies = 0;
	trigger_jiffies = 0;
	unlock = 0;
}

static int sec_debug_keyboard_call(struct notifier_block *this,
				   unsigned long type, void *data)
{
	struct keyboard_notifier_param *param = data;

	if (likely(type != KBD_KEYCODE && type != KBD_UNBOUND_KEYCODE))
		return NOTIFY_DONE;

	sec_debug_check_crash_key(param->value, param->down);

	return NOTIFY_DONE;
}

static struct notifier_block sec_debug_keyboard_notifier = {
	.notifier_call	= sec_debug_keyboard_call,
};

void __init sec_debug_init_crash_key(struct sec_crash_key *crash_key)
{
	int i;

	if (!sec_debug_level.en.kernel_fault)
		return;

	if (crash_key) {
		__sec_crash_key = crash_key;
		if (__sec_crash_key->timeout == 0)
			__sec_crash_key->timeout = dflt_crash_key.timeout;
	}

	__sec_crash_key->unlock = 0;
	for (i = 0; i < __sec_crash_key->size - 1; i++)
		__sec_crash_key->unlock |= 1 << i;

	register_keyboard_notifier(&sec_debug_keyboard_notifier);
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = sec_debug_normal_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = sec_debug_panic_handler,
};

static void sec_debug_set_build_info(void)
{
	char *p = gkernel_sec_build_info;
	sprintf(p, "Kernel Build Info : ");
	strcat(p, " Date:");
	strncat(p, gkernel_sec_build_info_date_time[0], 12);
	strcat(p, " Time:");
	strncat(p, gkernel_sec_build_info_date_time[1], 9);
}

static int __init sec_debug_init(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_set_build_info();

	sec_debug_set_upload_magic(SEC_DEBUG_MAGIC_DUMP);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

	register_reboot_notifier(&nb_reboot_block);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	return 0;
}

arch_initcall(sec_debug_init);

int sec_debug_get_level(void)
{
	return sec_debug_level.uint_val;
}

/* Exynos compatibility */
int get_sec_debug_level(void)
	__attribute__ ((weak, alias("sec_debug_get_level")));

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
void __sec_debug_task_log(int cpu, struct task_struct *task)
{
	unsigned i;

	if (likely(!sec_debug_level.en.kernel_fault))
		return;

	i = atomic_inc_return(&task_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->task[cpu][i].time = cpu_clock(cpu);
	strcpy(psec_debug_log->task[cpu][i].comm, task->comm);
	psec_debug_log->task[cpu][i].pid = task->pid;
}

void __sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (likely(!sec_debug_level.en.kernel_fault))
		return;

	i = atomic_inc_return(&irq_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->irq[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->irq[cpu][i].irq = irq;
	psec_debug_log->irq[cpu][i].fn = (void *)fn;
	psec_debug_log->irq[cpu][i].en = en;
}

void __sec_debug_work_log(struct worker *worker,
			  struct work_struct *work, work_func_t f)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (likely(!sec_debug_level.en.kernel_fault))
		return;

	i = atomic_inc_return(&work_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->work[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->work[cpu][i].worker = worker;
	psec_debug_log->work[cpu][i].work = work;
	psec_debug_log->work[cpu][i].f = f;
}
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
static unsigned long long excp_irq_exit_time[NR_CPUS];

void sec_debug_irq_last_exit_log(void)
{
	int cpu = raw_smp_processor_id();
	excp_irq_exit_time[cpu] = cpu_clock(cpu);
}
#endif /* CONFIG_SEC_DEBUG_IRQ_EXIT_LOG */
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
void debug_semaphore_init(void)
{
	int i = 0;
	struct sem_debug *sem_debug = NULL;

	spin_lock_init(&sem_debug_lock);
	sem_debug_free_head_cnt = 0;
	sem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&sem_debug_free_head.list);
	INIT_LIST_HEAD(&sem_debug_done_head.list);

	for (i = 0; i < SEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		sem_debug = kmalloc(sizeof(struct sem_debug), GFP_KERNEL);
		/* add list */
		list_add(&sem_debug->list, &sem_debug_free_head.list);
		sem_debug_free_head_cnt++;
	}

	sem_debug_init = 1;
}

void debug_semaphore_down_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &sem_debug_done_head.list);
		sem_debug_free_head_cnt--;
		sem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

void debug_semaphore_up_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &sem_debug_free_head.list);
			sem_debug_free_head_cnt++;
			sem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

/* rwsemaphore logging */
void debug_rwsemaphore_init(void)
{
	int i = 0;
	struct rwsem_debug *rwsem_debug = NULL;

	spin_lock_init(&rwsem_debug_lock);
	rwsem_debug_free_head_cnt = 0;
	rwsem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&rwsem_debug_free_head.list);
	INIT_LIST_HEAD(&rwsem_debug_done_head.list);

	for (i = 0; i < RWSEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		rwsem_debug = kmalloc(sizeof(struct rwsem_debug), GFP_KERNEL);
		/* add list */
		list_add(&rwsem_debug->list, &rwsem_debug_free_head.list);
		rwsem_debug_free_head_cnt++;
	}

	rwsem_debug_init = 1;
}

void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		sem_dbg->direction = dir;
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &rwsem_debug_done_head.list);
		rwsem_debug_free_head_cnt--;
		rwsem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}

void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &rwsem_debug_free_head.list);
			rwsem_debug_free_head_cnt++;
			rwsem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}
#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_USER
void sec_user_fault_dump(void)
{
	if (sec_debug_level.en.kernel_fault == 1 &&
	    sec_debug_level.en.user_fault == 1)
		panic("User Fault");
}

static int sec_user_fault_write(struct file *file, const char __user * buffer,
				size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_user_fault_dump();

	return count;
}

static const struct file_operations sec_user_fault_proc_fops = {
	.write = sec_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUGO, NULL,
			    &sec_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_user_fault_init);
#endif /* CONFIG_SEC_DEBUG_USER */
