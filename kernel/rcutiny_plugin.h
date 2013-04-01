/*
 * Read-Copy Update mechanism for mutual exclusion, the Bloatwatch edition
 * Internal non-public definitions that provide either classic
 * or preemptible semantics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (c) 2010 Linaro
 *
 * Author: Paul E. McKenney <paulmck@linux.vnet.ibm.com>
 */

#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

/* Global control variables for rcupdate callback mechanism. */
struct rcu_ctrlblk {
	struct rcu_head *rcucblist;	/* List of pending callbacks (CBs). */
	struct rcu_head **donetail;	/* ->next pointer of last "done" CB. */
	struct rcu_head **curtail;	/* ->next pointer of last CB. */
	RCU_TRACE(long qlen);		/* Number of pending CBs. */
	RCU_TRACE(unsigned long gp_start); /* Start time for stalls. */
	RCU_TRACE(unsigned long ticks_this_gp); /* Statistic for stalls. */
	RCU_TRACE(unsigned long jiffies_stall); /* Jiffies at next stall. */
	RCU_TRACE(char *name);		/* Name of RCU type. */
};

/* Definition for rcupdate control block. */
static struct rcu_ctrlblk rcu_sched_ctrlblk = {
	.donetail	= &rcu_sched_ctrlblk.rcucblist,
	.curtail	= &rcu_sched_ctrlblk.rcucblist,
	RCU_TRACE(.name = "rcu_sched")
};

static struct rcu_ctrlblk rcu_bh_ctrlblk = {
	.donetail	= &rcu_bh_ctrlblk.rcucblist,
	.curtail	= &rcu_bh_ctrlblk.rcucblist,
	RCU_TRACE(.name = "rcu_bh")
};

#ifdef CONFIG_DEBUG_LOCK_ALLOC
int rcu_scheduler_active __read_mostly;
EXPORT_SYMBOL_GPL(rcu_scheduler_active);
#endif /* #ifdef CONFIG_DEBUG_LOCK_ALLOC */

#ifdef CONFIG_RCU_TRACE

static void check_cpu_stall(struct rcu_ctrlblk *rcp)
{
	unsigned long j;
	unsigned long js;

	if (rcu_cpu_stall_suppress)
		return;
	rcp->ticks_this_gp++;
	j = jiffies;
	js = rcp->jiffies_stall;
	if (*rcp->curtail && ULONG_CMP_GE(j, js)) {
		pr_err("INFO: %s stall on CPU (%lu ticks this GP) idle=%llx (t=%lu jiffies q=%ld)\n",
		       rcp->name, rcp->ticks_this_gp, rcu_dynticks_nesting,
		       jiffies - rcp->gp_start, rcp->qlen);
		dump_stack();
	}
	if (*rcp->curtail && ULONG_CMP_GE(j, js))
		rcp->jiffies_stall = jiffies +
			3 * rcu_jiffies_till_stall_check() + 3;
	else if (ULONG_CMP_GE(j, js))
		rcp->jiffies_stall = jiffies + rcu_jiffies_till_stall_check();
}

static void check_cpu_stall_preempt(void);

#endif /* #ifdef CONFIG_RCU_TRACE */

static void reset_cpu_stall_ticks(struct rcu_ctrlblk *rcp)
{
#ifdef CONFIG_RCU_TRACE
	rcp->ticks_this_gp = 0;
	rcp->gp_start = jiffies;
	rcp->jiffies_stall = jiffies + rcu_jiffies_till_stall_check();
#endif /* #ifdef CONFIG_RCU_TRACE */
}

static void check_cpu_stalls(void)
{
	RCU_TRACE(check_cpu_stall(&rcu_bh_ctrlblk));
	RCU_TRACE(check_cpu_stall(&rcu_sched_ctrlblk));
	RCU_TRACE(check_cpu_stall_preempt());
}

#ifdef CONFIG_TINY_PREEMPT_RCU

#include <linux/delay.h>

/* Global control variables for preemptible RCU. */
struct rcu_preempt_ctrlblk {
	struct rcu_ctrlblk rcb;	/* curtail: ->next ptr of last CB for GP. */
	struct rcu_head **nexttail;
				/* Tasks blocked in a preemptible RCU */
				/*  read-side critical section while an */
				/*  preemptible-RCU grace period is in */
				/*  progress must wait for a later grace */
				/*  period.  This pointer points to the */
				/*  ->next pointer of the last task that */
				/*  must wait for a later grace period, or */
				/*  to &->rcb.rcucblist if there is no */
				/*  such task. */
	struct list_head blkd_tasks;
				/* Tasks blocked in RCU read-side critical */
				/*  section.  Tasks are placed at the head */
				/*  of this list and age towards the tail. */
	struct list_head *gp_tasks;
				/* Pointer to the first task blocking the */
				/*  current grace period, or NULL if there */
				/*  is no such task. */
	struct list_head *exp_tasks;
				/* Pointer to first task blocking the */
				/*  current expedited grace period, or NULL */
				/*  if there is no such task.  If there */
				/*  is no current expedited grace period, */
				/*  then there cannot be any such task. */
#ifdef CONFIG_RCU_BOOST
	struct list_head *boost_tasks;
				/* Pointer to first task that needs to be */
				/*  priority-boosted, or NULL if no priority */
				/*  boosting is needed.  If there is no */
				/*  current or expedited grace period, there */
				/*  can be no such task. */
#endif /* #ifdef CONFIG_RCU_BOOST */
	u8 gpnum;		/* Current grace period. */
	u8 gpcpu;		/* Last grace period blocked by the CPU. */
	u8 completed;		/* Last grace period completed. */
				/*  If all three are equal, RCU is idle. */
#ifdef CONFIG_RCU_BOOST
	unsigned long boost_time; /* When to start boosting (jiffies) */
#endif /* #ifdef CONFIG_RCU_BOOST */
#ifdef CONFIG_RCU_TRACE
	unsigned long n_grace_periods;
#ifdef CONFIG_RCU_BOOST
	unsigned long n_tasks_boosted;
				/* Total number of tasks boosted. */
	unsigned long n_exp_boosts;
				/* Number of tasks boosted for expedited GP. */
	unsigned long n_normal_boosts;
				/* Number of tasks boosted for normal GP. */
	unsigned long n_balk_blkd_tasks;
				/* Refused to boost: no blocked tasks. */
	unsigned long n_balk_exp_gp_tasks;
				/* Refused to boost: nothing blocking GP. */
	unsigned long n_balk_boost_tasks;
				/* Refused to boost: already boosting. */
	unsigned long n_balk_notyet;
				/* Refused to boost: not yet time. */
	unsigned long n_balk_nos;
				/* Refused to boost: not sure why, though. */
				/*  This can happen due to race conditions. */
#endif /* #ifdef CONFIG_RCU_BOOST */
#endif /* #ifdef CONFIG_RCU_TRACE */
};

static struct rcu_preempt_ctrlblk rcu_preempt_ctrlblk = {
	.rcb.donetail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.rcb.curtail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.nexttail = &rcu_preempt_ctrlblk.rcb.rcucblist,
	.blkd_tasks = LIST_HEAD_INIT(rcu_preempt_ctrlblk.blkd_tasks),
	RCU_TRACE(.rcb.name = "rcu_preempt")
};

static int rcu_preempted_readers_exp(void);
static void rcu_report_exp_done(void);

/*
 * Return true if the CPU has not yet responded to the current grace period.
 */
static int rcu_cpu_blocking_cur_gp(void)
{
	return rcu_preempt_ctrlblk.gpcpu != rcu_preempt_ctrlblk.gpnum;
}

/*
 * Check for a running RCU reader.  Because there is only one CPU,
 * there can be but one running RCU reader at a time.  ;-)
 *
 * Returns zero if there are no running readers.  Returns a positive
 * number if there is at least one reader within its RCU read-side
 * critical section.  Returns a negative number if an outermost reader
 * is in the midst of exiting from its RCU read-side critical section
 *
 * Returns zero if there are no running readers.  Returns a positive
 * number if there is at least one reader within its RCU read-side
 * critical section.  Returns a negative number if an outermost reader
 * is in the midst of exiting from its RCU read-side critical section.
 */
static int rcu_preempt_running_reader(void)
{
	return current->rcu_read_lock_nesting;
}

/*
 * Check for preempted RCU readers blocking any grace period.
 * If the caller needs a reliable answer, it must disable hard irqs.
 */
static int rcu_preempt_blocked_readers_any(void)
{
	return !list_empty(&rcu_preempt_ctrlblk.blkd_tasks);
}

/*
 * Check for preempted RCU readers blocking the current grace period.
 * If the caller needs a reliable answer, it must disable hard irqs.
 */
static int rcu_preempt_blocked_readers_cgp(void)
{
	return rcu_preempt_ctrlblk.gp_tasks != NULL;
}

/*
 * Return true if another preemptible-RCU grace period is needed.
 */
static int rcu_preempt_needs_another_gp(void)
{
	return *rcu_preempt_ctrlblk.rcb.curtail != NULL;
}

/*
 * Return true if a preemptible-RCU grace period is in progress.
 * The caller must disable hardirqs.
 */
static int rcu_preempt_gp_in_progress(void)
{
	return rcu_preempt_ctrlblk.completed != rcu_preempt_ctrlblk.gpnum;
}

/*
 * Advance a ->blkd_tasks-list pointer to the next entry, instead
 * returning NULL if at the end of the list.
 */
static struct list_head *rcu_next_node_entry(struct task_struct *t)
{
	struct list_head *np;

	np = t->rcu_node_entry.next;
	if (np == &rcu_preempt_ctrlblk.blkd_tasks)
		np = NULL;
	return np;
}

#ifdef CONFIG_RCU_TRACE

#ifdef CONFIG_RCU_BOOST
static void rcu_initiate_boost_trace(void);
#endif /* #ifdef CONFIG_RCU_BOOST */

/*
 * Dump additional statistice for TINY_PREEMPT_RCU.
 */
static void show_tiny_preempt_stats(struct seq_file *m)
{
	seq_printf(m, "rcu_preempt: qlen=%ld gp=%lu g%u/p%u/c%u tasks=%c%c%c\n",
		   rcu_preempt_ctrlblk.rcb.qlen,
		   rcu_preempt_ctrlblk.n_grace_periods,
		   rcu_preempt_ctrlblk.gpnum,
		   rcu_preempt_ctrlblk.gpcpu,
		   rcu_preempt_ctrlblk.completed,
		   "T."[list_empty(&rcu_preempt_ctrlblk.blkd_tasks)],
		   "N."[!rcu_preempt_ctrlblk.gp_tasks],
		   "E."[!rcu_preempt_ctrlblk.exp_tasks]);
#ifdef CONFIG_RCU_BOOST
	seq_printf(m, "%sttb=%c ntb=%lu neb=%lu nnb=%lu j=%04x bt=%04x\n",
		   "             ",
		   "B."[!rcu_preempt_ctrlblk.boost_tasks],
		   rcu_preempt_ctrlblk.n_tasks_boosted,
		   rcu_preempt_ctrlblk.n_exp_boosts,
		   rcu_preempt_ctrlblk.n_normal_boosts,
		   (int)(jiffies & 0xffff),
		   (int)(rcu_preempt_ctrlblk.boost_time & 0xffff));
	seq_printf(m, "%s: nt=%lu egt=%lu bt=%lu ny=%lu nos=%lu\n",
		   "             balk",
		   rcu_preempt_ctrlblk.n_balk_blkd_tasks,
		   rcu_preempt_ctrlblk.n_balk_exp_gp_tasks,
		   rcu_preempt_ctrlblk.n_balk_boost_tasks,
		   rcu_preempt_ctrlblk.n_balk_notyet,
		   rcu_preempt_ctrlblk.n_balk_nos);
#endif /* #ifdef CONFIG_RCU_BOOST */
}

#endif /* #ifdef CONFIG_RCU_TRACE */

#ifdef CONFIG_RCU_BOOST

#include "rtmutex_common.h"

#define RCU_BOOST_PRIO CONFIG_RCU_BOOST_PRIO

/* Controls for rcu_kthread() kthread. */
static struct task_struct *rcu_kthread_task;
static DECLARE_WAIT_QUEUE_HEAD(rcu_kthread_wq);
static unsigned long have_rcu_kthread_work;

/*
 * Carry out RCU priority boosting on the task indicated by ->boost_tasks,
 * and advance ->boost_tasks to the next task in the ->blkd_tasks list.
 */
static int rcu_boost(void)
{
	unsigned long flags;
	struct rt_mutex mtx;
	struct task_struct *t;
	struct list_head *tb;

	if (rcu_preempt_ctrlblk.boost_tasks == NULL &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL)
		return 0;  /* Nothing to boost. */

	local_irq_save(flags);

	/*
	 * Recheck with irqs disabled: all tasks in need of boosting
	 * might exit their RCU read-side critical sections on their own
	 * if we are preempted just before disabling irqs.
	 */
	if (rcu_preempt_ctrlblk.boost_tasks == NULL &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL) {
		local_irq_restore(flags);
		return 0;
	}

	/*
	 * Preferentially boost tasks blocking expedited grace periods.
	 * This cannot starve the normal grace periods because a second
	 * expedited grace period must boost all blocked tasks, including
	 * those blocking the pre-existing normal grace period.
	 */
	if (rcu_preempt_ctrlblk.exp_tasks != NULL) {
		tb = rcu_preempt_ctrlblk.exp_tasks;
		RCU_TRACE(rcu_preempt_ctrlblk.n_exp_boosts++);
	} else {
		tb = rcu_preempt_ctrlblk.boost_tasks;
		RCU_TRACE(rcu_preempt_ctrlblk.n_normal_boosts++);
	}
	RCU_TRACE(rcu_preempt_ctrlblk.n_tasks_boosted++);

	/*
	 * We boost task t by manufacturing an rt_mutex that appears to
	 * be held by task t.  We leave a pointer to that rt_mutex where
	 * task t can find it, and task t will release the mutex when it
	 * exits its outermost RCU read-side critical section.  Then
	 * simply acquiring this artificial rt_mutex will boost task
	 * t's priority.  (Thanks to tglx for suggesting this approach!)
	 */
	t = container_of(tb, struct task_struct, rcu_node_entry);
	rt_mutex_init_proxy_locked(&mtx, t);
	t->rcu_boost_mutex = &mtx;
	local_irq_restore(flags);
	rt_mutex_lock(&mtx);
	rt_mutex_unlock(&mtx);  /* Keep lockdep happy. */

	return ACCESS_ONCE(rcu_preempt_ctrlblk.boost_tasks) != NULL ||
	       ACCESS_ONCE(rcu_preempt_ctrlblk.exp_tasks) != NULL;
}

/*
 * Check to see if it is now time to start boosting RCU readers blocking
 * the current grace period, and, if so, tell the rcu_kthread_task to
 * start boosting them.  If there is an expedited boost in progress,
 * we wait for it to complete.
 *
 * If there are no blocked readers blocking the current grace period,
 * return 0 to let the caller know, otherwise return 1.  Note that this
 * return value is independent of whether or not boosting was done.
 */
static int rcu_initiate_boost(void)
{
	if (!rcu_preempt_blocked_readers_cgp() &&
	    rcu_preempt_ctrlblk.exp_tasks == NULL) {
		RCU_TRACE(rcu_preempt_ctrlblk.n_balk_exp_gp_tasks++);
		return 0;
	}
	if (rcu_preempt_ctrlblk.exp_tasks != NULL ||
	    (rcu_preempt_ctrlblk.gp_tasks != NULL &&
	     rcu_preempt_ctrlblk.boost_tasks == NULL &&
	     ULONG_CMP_GE(jiffies, rcu_preempt_ctrlblk.boost_time))) {
		if (rcu_preempt_ctrlblk.exp_tasks == NULL)
			rcu_preempt_ctrlblk.boost_tasks =
				rcu_preempt_ctrlblk.gp_tasks;
		invoke_rcu_callbacks();
	} else {
		RCU_TRACE(rcu_initiate_boost_trace());
	}
	return 1;
}

#define RCU_BOOST_DELAY_JIFFIES DIV_ROUND_UP(CONFIG_RCU_BOOST_DELAY * HZ, 1000)

/*
 * Do priority-boost accounting for the start of a new grace period.
 */
static void rcu_preempt_boost_start_gp(void)
{
	rcu_preempt_ctrlblk.boost_time = jiffies + RCU_BOOST_DELAY_JIFFIES;
}

#else /* #ifdef CONFIG_RCU_BOOST */

/*
 * If there is no RCU priority boosting, we don't initiate boosting,
 * but we do indicate whether there are blocked readers blocking the
 * current grace period.
 */
static int rcu_initiate_boost(void)
{
	return rcu_preempt_blocked_readers_cgp();
}

/*
 * If there is no RCU priority boosting, nothing to do at grace-period start.
 */
static void rcu_preempt_boost_start_gp(void)
{
}

#endif /* else #ifdef CONFIG_RCU_BOOST */

/*
 * Record a preemptible-RCU quiescent state for the specified CPU.  Note
 * that this just means that the task currently running on the CPU is
 * in a quiescent state.  There might be any number of tasks blocked
 * while in an RCU read-side critical section.
 *
 * Unlike the other rcu_*_qs() functions, callers to this function
 * must disable irqs in order to protect the assignment to
 * ->rcu_read_unlock_special.
 *
 * Because this is a single-CPU implementation, the only way a grace
 * period can end is if the CPU is in a quiescent state.  The reason is
 * that a blocked preemptible-RCU reader can exit its critical section
 * only if the CPU is running it at the time.  Therefore, when the
 * last task blocking the current grace period exits its RCU read-side
 * critical section, neither the CPU nor blocked tasks will be stopping
 * the current grace period.  (In contrast, SMP implementations
 * might have CPUs running in RCU read-side critical sections that
 * block later grace periods -- but this is not possible given only
 * one CPU.)
 */
static void rcu_preempt_cpu_qs(void)
{
	/* Record both CPU and task as having responded to current GP. */
	rcu_preempt_ctrlblk.gpcpu = rcu_preempt_ctrlblk.gpnum;
	current->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_NEED_QS;

	/* If there is no GP then there is nothing more to do.  */
	if (!rcu_preempt_gp_in_progress())
		return;
	/*
	 * Check up on boosting.  If there are readers blocking the
	 * current grace period, leave.
	 */
	if (rcu_initiate_boost())
		return;

	/* Advance callbacks. */
	rcu_preempt_ctrlblk.completed = rcu_preempt_ctrlblk.gpnum;
	rcu_preempt_ctrlblk.rcb.donetail = rcu_preempt_ctrlblk.rcb.curtail;
	rcu_preempt_ctrlblk.rcb.curtail = rcu_preempt_ctrlblk.nexttail;

	/* If there are no blocked readers, next GP is done instantly. */
	if (!rcu_preempt_blocked_readers_any())
		rcu_preempt_ctrlblk.rcb.donetail = rcu_preempt_ctrlblk.nexttail;

	/* If there are done callbacks, cause them to be invoked. */
	if (*rcu_preempt_ctrlblk.rcb.donetail != NULL)
		invoke_rcu_callbacks();
}

/*
 * Start a new RCU grace period if warranted.  Hard irqs must be disabled.
 */
static void rcu_preempt_start_gp(void)
{
	if (!rcu_preempt_gp_in_progress() && rcu_preempt_needs_another_gp()) {

		/* Official start of GP. */
		rcu_preempt_ctrlblk.gpnum++;
		RCU_TRACE(rcu_preempt_ctrlblk.n_grace_periods++);
		reset_cpu_stall_ticks(&rcu_preempt_ctrlblk.rcb);

		/* Any blocked RCU readers block new GP. */
		if (rcu_preempt_blocked_readers_any())
			rcu_preempt_ctrlblk.gp_tasks =
				rcu_preempt_ctrlblk.blkd_tasks.next;

		/* Set up for RCU priority boosting. */
		rcu_preempt_boost_start_gp();

		/* If there is no running reader, CPU is done with GP. */
		if (!rcu_preempt_running_reader())
			rcu_preempt_cpu_qs();
	}
}

/*
 * We have entered the scheduler, and the current task might soon be
 * context-switched away from.  If this task is in an RCU read-side
 * critical section, we will no longer be able to rely on the CPU to
 * record that fact, so we enqueue the task on the blkd_tasks list.
 * If the task started after the current grace period began, as recorded
 * by ->gpcpu, we enqueue at the beginning of the list.  Otherwise
 * before the element referenced by ->gp_tasks (or at the tail if
 * ->gp_tasks is NULL) and point ->gp_tasks at the newly added element.
 * The task will dequeue itself when it exits the outermost enclosing
 * RCU read-side critical section.  Therefore, the current grace period
 * cannot be permitted to complete until the ->gp_tasks pointer becomes
 * NULL.
 *
 * Caller must disable preemption.
 */
void rcu_preempt_note_context_switch(void)
{
	struct task_struct *t = current;
	unsigned long flags;

	local_irq_save(flags); /* must exclude scheduler_tick(). */
	if (rcu_preempt_running_reader() > 0 &&
	    (t->rcu_read_unlock_special & RCU_READ_UNLOCK_BLOCKED) == 0) {

		/* Possibly blocking in an RCU read-side critical section. */
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_BLOCKED;

		/*
		 * If this CPU has already checked in, then this task
		 * will hold up the next grace period rather than the
		 * current grace period.  Queue the task accordingly.
		 * If the task is queued for the current grace period
		 * (i.e., this CPU has not yet passed through a quiescent
		 * state for the current grace period), then as long
		 * as that task remains queued, the current grace period
		 * cannot end.
		 */
		list_add(&t->rcu_node_entry, &rcu_preempt_ctrlblk.blkd_tasks);
		if (rcu_cpu_blocking_cur_gp())
			rcu_preempt_ctrlblk.gp_tasks = &t->rcu_node_entry;
	} else if (rcu_preempt_running_reader() < 0 &&
		   t->rcu_read_unlock_special) {
		/*
		 * Complete exit from RCU read-side critical section on
		 * behalf of preempted instance of __rcu_read_unlock().
		 */
		rcu_read_unlock_special(t);
	}

	/*
	 * Either we were not in an RCU read-side critical section to
	 * begin with, or we have now recorded that critical section
	 * globally.  Either way, we can now note a quiescent state
	 * for this CPU.  Again, if we were in an RCU read-side critical
	 * section, and if that critical section was blocking the current
	 * grace period, then the fact that the task has been enqueued
	 * means that current grace period continues to be blocked.
	 */
	rcu_preempt_cpu_qs();
	local_irq_restore(flags);
}

/*
 * Handle special cases during rcu_read_unlock(), such as needing to
 * notify RCU core processing or task having blocked during the RCU
 * read-side critical section.
 */
void rcu_read_unlock_special(struct task_struct *t)
{
	int empty;
	int empty_exp;
	unsigned long flags;
	struct list_head *np;
#ifdef CONFIG_RCU_BOOST
	struct rt_mutex *rbmp = NULL;
#endif /* #ifdef CONFIG_RCU_BOOST */
	int special;

	/*
	 * NMI handlers cannot block and cannot safely manipulate state.
	 * They therefore cannot possibly be special, so just leave.
	 */
	if (in_nmi())
		return;

	local_irq_save(flags);

	/*
	 * If RCU core is waiting for this CPU to exit critical section,
	 * let it know that we have done so.
	 */
	special = t->rcu_read_unlock_special;
	if (special & RCU_READ_UNLOCK_NEED_QS)
		rcu_preempt_cpu_qs();

	/* Hardware IRQ handlers cannot block. */
	if (in_irq() || in_serving_softirq()) {
		local_irq_restore(flags);
		return;
	}

	/* Clean up if blocked during RCU read-side critical section. */
	if (special & RCU_READ_UNLOCK_BLOCKED) {
		t->rcu_read_unlock_special &= ~RCU_READ_UNLOCK_BLOCKED;

		/*
		 * Remove this task from the ->blkd_tasks list and adjust
		 * any pointers that might have been referencing it.
		 */
		empty = !rcu_preempt_blocked_readers_cgp();
		empty_exp = rcu_preempt_ctrlblk.exp_tasks == NULL;
		np = rcu_next_node_entry(t);
		list_del_init(&t->rcu_node_entry);
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.gp_tasks)
			rcu_preempt_ctrlblk.gp_tasks = np;
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.exp_tasks)
			rcu_preempt_ctrlblk.exp_tasks = np;
#ifdef CONFIG_RCU_BOOST
		if (&t->rcu_node_entry == rcu_preempt_ctrlblk.boost_tasks)
			rcu_preempt_ctrlblk.boost_tasks = np;
#endif /* #ifdef CONFIG_RCU_BOOST */

		/*
		 * If this was the last task on the current list, and if
		 * we aren't waiting on the CPU, report the quiescent state
		 * and start a new grace period if needed.
		 */
		if (!empty && !rcu_preempt_blocked_readers_cgp()) {
			rcu_preempt_cpu_qs();
			rcu_preempt_start_gp();
		}

		/*
		 * If this was the last task on the expedited lists,
		 * then we need wake up the waiting task.
		 */
		if (!empty_exp && rcu_preempt_ctrlblk.exp_tasks == NULL)
			rcu_report_exp_done();
	}
#ifdef CONFIG_RCU_BOOST
	/* Unboost self if was boosted. */
	if (t->rcu_boost_mutex != NULL) {
		rbmp = t->rcu_boost_mutex;
		t->rcu_boost_mutex = NULL;
		rt_mutex_unlock(rbmp);
	}
#endif /* #ifdef CONFIG_RCU_BOOST */
	local_irq_restore(flags);
}

/*
 * Check for a quiescent state from the current CPU.  When a task blocks,
 * the task is recorded in the rcu_preempt_ctrlblk structure, which is
 * checked elsewhere.  This is called from the scheduling-clock interrupt.
 *
 * Caller must disable hard irqs.
 */
static void rcu_preempt_check_callbacks(void)
{
	struct task_struct *t = current;

	if (rcu_preempt_gp_in_progress() &&
	    (!rcu_preempt_running_reader() ||
	     !rcu_cpu_blocking_cur_gp()))
		rcu_preempt_cpu_qs();
	if (&rcu_preempt_ctrlblk.rcb.rcucblist !=
	    rcu_preempt_ctrlblk.rcb.donetail)
		invoke_rcu_callbacks();
	if (rcu_preempt_gp_in_progress() &&
	    rcu_cpu_blocking_cur_gp() &&
	    rcu_preempt_running_reader() > 0)
		t->rcu_read_unlock_special |= RCU_READ_UNLOCK_NEED_QS;
}

/*
 * TINY_PREEMPT_RCU has an extra callback-list tail pointer to
 * update, so this is invoked from rcu_process_callbacks() to
 * handle that case.  Of course, it is invoked for all flavors of
 * RCU, but RCU callbacks can appear only on one of the lists, and
 * neither ->nexttail nor ->donetail can possibly be NULL, so there
 * is no need for an explicit check.
 */
static void rcu_preempt_remove_callbacks(struct rcu_ctrlblk *rcp)
{
	if (rcu_preempt_ctrlblk.nexttail == rcp->donetail)
		rcu_preempt_ctrlblk.nexttail = &rcp->rcucblist;
}

/*
 * Process callbacks for preemptible RCU.
 */
static void rcu_preempt_process_callbacks(void)
{
	__rcu_process_callbacks(&rcu_preempt_ctrlblk.rcb);
}

/*
 * Queue a preemptible -RCU callback for invocation after a grace period.
 */
void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu))
{
	unsigned long flags;

	debug_rcu_head_queue(head);
	head->func = func;
	head->next = NULL;

	local_irq_save(flags);
	*rcu_preempt_ctrlblk.nexttail = head;
	rcu_preempt_ctrlblk.nexttail = &head->next;
	RCU_TRACE(rcu_preempt_ctrlblk.rcb.qlen++);
	rcu_preempt_start_gp();  /* checks to see if GP needed. */
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(call_rcu);

/*
 * synchronize_rcu - wait until a grace period has elapsed.
 *
 * Control will return to the caller some time after a full grace
 * period has elapsed, in other words after all currently executing RCU
 * read-side critical sections have completed.  RCU read-side critical
 * sections are delimited by rcu_read_lock() and rcu_read_unlock(),
 * and may be nested.
 */
void synchronize_rcu(void)
{
	rcu_lockdep_assert(!lock_is_held(&rcu_bh_lock_map) &&
			   !lock_is_held(&rcu_lock_map) &&
			   !lock_is_held(&rcu_sched_lock_map),
			   "Illegal synchronize_rcu() in RCU read-side critical section");

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	if (!rcu_scheduler_active)
		return;
#endif /* #ifdef CONFIG_DEBUG_LOCK_ALLOC */

	WARN_ON_ONCE(rcu_preempt_running_reader());
	if (!rcu_preempt_blocked_readers_any())
		return;

	/* Once we get past the fastpath checks, same code as rcu_barrier(). */
	if (rcu_expedited)
		synchronize_rcu_expedited();
	else
		rcu_barrier();
}
EXPORT_SYMBOL_GPL(synchronize_rcu);

static DECLARE_WAIT_QUEUE_HEAD(sync_rcu_preempt_exp_wq);
static unsigned long sync_rcu_preempt_exp_count;
static DEFINE_MUTEX(sync_rcu_preempt_exp_mutex);

/*
 * Return non-zero if there are any tasks in RCU read-side critical
 * sections blocking the current preemptible-RCU expedited grace period.
 * If there is no preemptible-RCU expedited grace period currently in
 * progress, returns zero unconditionally.
 */
static int rcu_preempted_readers_exp(void)
{
	return rcu_preempt_ctrlblk.exp_tasks != NULL;
}

/*
 * Report the exit from RCU read-side critical section for the last task
 * that queued itself during or before the current expedited preemptible-RCU
 * grace period.
 */
static void rcu_report_exp_done(void)
{
	wake_up(&sync_rcu_preempt_exp_wq);
}

/*
 * Wait for an rcu-preempt grace period, but expedite it.  The basic idea
 * is to rely in the fact that there is but one CPU, and that it is
 * illegal for a task to invoke synchronize_rcu_expedited() while in a
 * preemptible-RCU read-side critical section.  Therefore, any such
 * critical sections must correspond to blocked tasks, which must therefore
 * be on the ->blkd_tasks list.  So just record the current head of the
 * list in the ->exp_tasks pointer, and wait for all tasks including and
 * after the task pointed to by ->exp_tasks to drain.
 */
void synchronize_rcu_expedited(void)
{
	unsigned long flags;
	struct rcu_preempt_ctrlblk *rpcp = &rcu_preempt_ctrlblk;
	unsigned long snap;

	barrier(); /* ensure prior action seen before grace period. */

	WARN_ON_ONCE(rcu_preempt_running_reader());

	/*
	 * Acquire lock so that there is only one preemptible RCU grace
	 * period in flight.  Of course, if someone does the expedited
	 * grace period for us while we are acquiring the lock, just leave.
	 */
	snap = sync_rcu_preempt_exp_count + 1;
	mutex_lock(&sync_rcu_preempt_exp_mutex);
	if (ULONG_CMP_LT(snap, sync_rcu_preempt_exp_count))
		goto unlock_mb_ret; /* Others did our work for us. */

	local_irq_save(flags);

	/*
	 * All RCU readers have to already be on blkd_tasks because
	 * we cannot legally be executing in an RCU read-side critical
	 * section.
	 */

	/* Snapshot current head of ->blkd_tasks list. */
	rpcp->exp_tasks = rpcp->blkd_tasks.next;
	if (rpcp->exp_tasks == &rpcp->blkd_tasks)
		rpcp->exp_tasks = NULL;

	/* Wait for tail of ->blkd_tasks list to drain. */
	if (!rcu_preempted_readers_exp()) {
		local_irq_restore(flags);
	} else {
		rcu_initiate_boost();
		local_irq_restore(flags);
		wait_event(sync_rcu_preempt_exp_wq,
			   !rcu_preempted_readers_exp());
	}

	/* Clean up and exit. */
	barrier(); /* ensure expedited GP seen before counter increment. */
	sync_rcu_preempt_exp_count++;
unlock_mb_ret:
	mutex_unlock(&sync_rcu_preempt_exp_mutex);
	barrier(); /* ensure subsequent action seen after grace period. */
}
EXPORT_SYMBOL_GPL(synchronize_rcu_expedited);

/*
 * Does preemptible RCU need the CPU to stay out of dynticks mode?
 */
int rcu_preempt_needs_cpu(void)
{
	return rcu_preempt_ctrlblk.rcb.rcucblist != NULL;
}

#else /* #ifdef CONFIG_TINY_PREEMPT_RCU */

#ifdef CONFIG_RCU_TRACE

/*
 * Because preemptible RCU does not exist, it is not necessary to
 * dump out its statistics.
 */
static void show_tiny_preempt_stats(struct seq_file *m)
{
}

#endif /* #ifdef CONFIG_RCU_TRACE */

/*
 * Because preemptible RCU does not exist, it never has any callbacks
 * to check.
 */
static void rcu_preempt_check_callbacks(void)
{
}

/*
 * Because preemptible RCU does not exist, it never has any callbacks
 * to remove.
 */
static void rcu_preempt_remove_callbacks(struct rcu_ctrlblk *rcp)
{
}

/*
 * Because preemptible RCU does not exist, it never has any callbacks
 * to process.
 */
static void rcu_preempt_process_callbacks(void)
{
}

#endif /* #else #ifdef CONFIG_TINY_PREEMPT_RCU */

#ifdef CONFIG_RCU_BOOST

/*
 * Wake up rcu_kthread() to process callbacks now eligible for invocation
 * or to boost readers.
 */
static void invoke_rcu_callbacks(void)
{
	have_rcu_kthread_work = 1;
	if (rcu_kthread_task != NULL)
		wake_up(&rcu_kthread_wq);
}

#ifdef CONFIG_RCU_TRACE

/*
 * Is the current CPU running the RCU-callbacks kthread?
 * Caller must have preemption disabled.
 */
static bool rcu_is_callbacks_kthread(void)
{
	return rcu_kthread_task == current;
}

#endif /* #ifdef CONFIG_RCU_TRACE */

/*
 * This kthread invokes RCU callbacks whose grace periods have
 * elapsed.  It is awakened as needed, and takes the place of the
 * RCU_SOFTIRQ that is used for this purpose when boosting is disabled.
 * This is a kthread, but it is never stopped, at least not until
 * the system goes down.
 */
static int rcu_kthread(void *arg)
{
	unsigned long work;
	unsigned long morework;
	unsigned long flags;

	for (;;) {
		wait_event_interruptible(rcu_kthread_wq,
					 have_rcu_kthread_work != 0);
		morework = rcu_boost();
		local_irq_save(flags);
		work = have_rcu_kthread_work;
		have_rcu_kthread_work = morework;
		local_irq_restore(flags);
		if (work)
			rcu_process_callbacks(NULL);
		schedule_timeout_interruptible(1); /* Leave CPU for others. */
	}

	return 0;  /* Not reached, but needed to shut gcc up. */
}

/*
 * Spawn the kthread that invokes RCU callbacks.
 */
static int __init rcu_spawn_kthreads(void)
{
	struct sched_param sp;

	rcu_kthread_task = kthread_run(rcu_kthread, NULL, "rcu_kthread");
	sp.sched_priority = RCU_BOOST_PRIO;
	sched_setscheduler_nocheck(rcu_kthread_task, SCHED_FIFO, &sp);
	return 0;
}
early_initcall(rcu_spawn_kthreads);

#else /* #ifdef CONFIG_RCU_BOOST */

/* Hold off callback invocation until early_initcall() time. */
static int rcu_scheduler_fully_active __read_mostly;

/*
 * Start up softirq processing of callbacks.
 */
void invoke_rcu_callbacks(void)
{
	if (rcu_scheduler_fully_active)
		raise_softirq(RCU_SOFTIRQ);
}

#ifdef CONFIG_RCU_TRACE

/*
 * There is no callback kthread, so this thread is never it.
 */
static bool rcu_is_callbacks_kthread(void)
{
	return false;
}

#endif /* #ifdef CONFIG_RCU_TRACE */

static int __init rcu_scheduler_really_started(void)
{
	rcu_scheduler_fully_active = 1;
	open_softirq(RCU_SOFTIRQ, rcu_process_callbacks);
	raise_softirq(RCU_SOFTIRQ);  /* Invoke any callbacks from early boot. */
	return 0;
}
early_initcall(rcu_scheduler_really_started);

#endif /* #else #ifdef CONFIG_RCU_BOOST */

#ifdef CONFIG_DEBUG_LOCK_ALLOC
#include <linux/kernel_stat.h>

/*
 * During boot, we forgive RCU lockdep issues.  After this function is
 * invoked, we start taking RCU lockdep issues seriously.
 */
void __init rcu_scheduler_starting(void)
{
	WARN_ON(nr_context_switches() > 0);
	rcu_scheduler_active = 1;
}

#endif /* #ifdef CONFIG_DEBUG_LOCK_ALLOC */

#ifdef CONFIG_RCU_TRACE

#ifdef CONFIG_RCU_BOOST

static void rcu_initiate_boost_trace(void)
{
	if (list_empty(&rcu_preempt_ctrlblk.blkd_tasks))
		rcu_preempt_ctrlblk.n_balk_blkd_tasks++;
	else if (rcu_preempt_ctrlblk.gp_tasks == NULL &&
		 rcu_preempt_ctrlblk.exp_tasks == NULL)
		rcu_preempt_ctrlblk.n_balk_exp_gp_tasks++;
	else if (rcu_preempt_ctrlblk.boost_tasks != NULL)
		rcu_preempt_ctrlblk.n_balk_boost_tasks++;
	else if (!ULONG_CMP_GE(jiffies, rcu_preempt_ctrlblk.boost_time))
		rcu_preempt_ctrlblk.n_balk_notyet++;
	else
		rcu_preempt_ctrlblk.n_balk_nos++;
}

#endif /* #ifdef CONFIG_RCU_BOOST */

static void rcu_trace_sub_qlen(struct rcu_ctrlblk *rcp, int n)
{
	unsigned long flags;

	local_irq_save(flags);
	rcp->qlen -= n;
	local_irq_restore(flags);
}

/*
 * Dump statistics for TINY_RCU, such as they are.
 */
static int show_tiny_stats(struct seq_file *m, void *unused)
{
	show_tiny_preempt_stats(m);
	seq_printf(m, "rcu_sched: qlen: %ld\n", rcu_sched_ctrlblk.qlen);
	seq_printf(m, "rcu_bh: qlen: %ld\n", rcu_bh_ctrlblk.qlen);
	return 0;
}

static int show_tiny_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_tiny_stats, NULL);
}

static const struct file_operations show_tiny_stats_fops = {
	.owner = THIS_MODULE,
	.open = show_tiny_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *rcudir;

static int __init rcutiny_trace_init(void)
{
	struct dentry *retval;

	rcudir = debugfs_create_dir("rcu", NULL);
	if (!rcudir)
		goto free_out;
	retval = debugfs_create_file("rcudata", 0444, rcudir,
				     NULL, &show_tiny_stats_fops);
	if (!retval)
		goto free_out;
	return 0;
free_out:
	debugfs_remove_recursive(rcudir);
	return 1;
}

static void __exit rcutiny_trace_cleanup(void)
{
	debugfs_remove_recursive(rcudir);
}

module_init(rcutiny_trace_init);
module_exit(rcutiny_trace_cleanup);

MODULE_AUTHOR("Paul E. McKenney");
MODULE_DESCRIPTION("Read-Copy Update tracing for tiny implementation");
MODULE_LICENSE("GPL");

static void check_cpu_stall_preempt(void)
{
#ifdef CONFIG_TINY_PREEMPT_RCU
	check_cpu_stall(&rcu_preempt_ctrlblk.rcb);
#endif /* #ifdef CONFIG_TINY_PREEMPT_RCU */
}

#endif /* #ifdef CONFIG_RCU_TRACE */
