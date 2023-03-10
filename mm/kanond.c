// SPDX-License-Identifier: GPL-2.0
/*
 * linux/mm/kanond.c
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 */
#include <uapi/linux/sched/types.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/jiffies.h>

#define KANOND_NR_RECLAIM 	SWAP_CLUSTER_MAX
#define STR_BUF_SIZE	100
#define KANOND_WMARK_GAP	10UL	/* 10 MB*/
#define KANOND_SWAP_GAP		50UL	/* 50 MB*/

#define GB_TO_PAGES(x) ((x) << (30 - PAGE_SHIFT))
#define MB_TO_PAGES(x) ((x) << (20 - PAGE_SHIFT))
#define M(x) ((x) >> (20 - PAGE_SHIFT))
#define K(x) ((x) << (PAGE_SHIFT-10))

static struct task_struct *task_kanond;
DECLARE_WAIT_QUEUE_HEAD(kanond_wait);

static unsigned long kanond_totalram_tbl[] = {
	GB_TO_PAGES(3),
	GB_TO_PAGES(4),
	GB_TO_PAGES(6),
	GB_TO_PAGES(8),
	GB_TO_PAGES(12),
};
static unsigned long kanond_min_anon_tbl[] = {
	MB_TO_PAGES(300),	/* <= 3GB */
	MB_TO_PAGES(300),	/* <= 4GB */
	MB_TO_PAGES(400),	/* <= 6GB */
	MB_TO_PAGES(500),	/* <= 8GB */
	MB_TO_PAGES(700),	/* <= 12GB */
};

static unsigned long kanond_wmark_tbl[] = {
	MB_TO_PAGES(768),	/* <= 3GB */
	MB_TO_PAGES(1024),	/* <= 4GB */
	MB_TO_PAGES(1536),	/* <= 6GB */
	MB_TO_PAGES(2048),	/* <= 8GB */
	MB_TO_PAGES(2048),	/* <= 12GB */
};

unsigned long boot_jiffies;
#ifdef CONFIG_BOOT_MEMORY_PARAM
bool need_boot_params = true;
#else
bool need_boot_params = false;
#endif

static const char * const reason_text[] = {
	"too small anon",
	"swap full",
	"enough available size",
	"mem boost period",
};

enum reason_enum {
	TOO_SMALL_ANON = 0,
	SWAP_FULL,
	ENOUGH_AVAIL,
	MEM_BOOST,
};

static const char *balanced_reason;

static int kanond_wmark_high_force = MB_TO_PAGES(CONFIG_KANOND_FORCE_SIZE);
static unsigned long kanond_min_anon __read_mostly;
static unsigned long kanond_wmark_high __read_mostly;
static unsigned long kanond_wmark_low __read_mostly;

const char *get_kanond_balanced_reason(void)
{
	return balanced_reason;
}

unsigned long get_kanond_wmark_high(void)
{
	return kanond_wmark_high;
}

int set_kanond_wmark_high(unsigned long wmark)
{

	if (wmark > totalram_pages)
		return -EINVAL;
	kanond_wmark_high = wmark;
	kanond_wmark_low = kanond_wmark_high
				- MB_TO_PAGES(KANOND_WMARK_GAP);
	wakeup_kanond();
	return 0;
}

static inline bool too_small_anon(void)
{
	unsigned long inactive_anon, active_anon;

	inactive_anon = global_node_page_state(NR_INACTIVE_ANON);
	active_anon = global_node_page_state(NR_ACTIVE_ANON);

	if (inactive_anon + active_anon < kanond_min_anon)
		return true;
	else
		return false;
}

static inline bool is_swap_full(void)
{
	unsigned long freeswap_pages;

	freeswap_pages = atomic_long_read(&nr_swap_pages);
	if (freeswap_pages < MB_TO_PAGES(KANOND_SWAP_GAP))
		return true;
	else
		return false;
}

static inline bool has_enough_avail(void)
{
	unsigned long free_pages, inactive_file, active_file;
	unsigned long available;

	free_pages = global_zone_page_state(NR_FREE_PAGES);
	inactive_file = global_node_page_state(NR_INACTIVE_FILE);
	active_file = global_node_page_state(NR_ACTIVE_FILE);
	available = free_pages + inactive_file + active_file;

	if (current == task_kanond && available >= kanond_wmark_high)
		return true;
	if (current != task_kanond && available >= kanond_wmark_low)
		return true;
	return false;
}

static inline void set_balanced_reason(enum reason_enum reason)
{
	balanced_reason = reason_text[reason];
}

static bool kanond_balanced(bool need_kanond_reason)
{
#ifndef CONFIG_NEED_MULTIPLE_NODES
	if (need_memory_boosting(&contig_page_data)) {
		if (need_kanond_reason)
			set_balanced_reason(MEM_BOOST);
		return true;
	}
#endif
	if (too_small_anon()) {
		if (need_kanond_reason)
			set_balanced_reason(TOO_SMALL_ANON);
		return true;
	}

	if (is_swap_full()) {
		if (need_kanond_reason)
			set_balanced_reason(SWAP_FULL);
		return true;
	}

	if (has_enough_avail()) {
		if (need_kanond_reason)
			set_balanced_reason(ENOUGH_AVAIL);
		return true;
	}

	return false;
}

void wakeup_kanond(void)
{
	if (current == task_kanond)
		return;
	if (!waitqueue_active(&kanond_wait))
		return;
	if (kanond_balanced(false))
		return;
	wake_up_interruptible(&kanond_wait);
}

void show_meminfo(char *str)
{
	unsigned long free_pages, active_file, inactive_file;
	unsigned long active_anon, inactive_anon, freeswap_pages;
	unsigned long available;

	free_pages = global_zone_page_state(NR_FREE_PAGES);
	inactive_file = global_node_page_state(NR_INACTIVE_FILE);
	active_file = global_node_page_state(NR_ACTIVE_FILE);
	inactive_anon = global_node_page_state(NR_INACTIVE_ANON);
	active_anon = global_node_page_state(NR_ACTIVE_ANON);
	freeswap_pages = atomic_long_read(&nr_swap_pages);
	available = free_pages + inactive_file + active_file;

	trace_printk("a: %lu < %lu < %lu FFaiAaiSft:%lu|%lu|%lu|%lu|%lu|%lu|%lu MB : %s\n",
		M(kanond_wmark_low), M(available), M(kanond_wmark_high),
		M(free_pages), M(active_file), M(inactive_file), M(active_anon),
		M(inactive_anon), M(freeswap_pages), M(total_swap_pages), str);
}

static bool kanond_try_to_sleep(void)
{
	long remaining = 0;
	DEFINE_WAIT(wait);
	bool did_short_sleep = false;

	prepare_to_wait(&kanond_wait, &wait, TASK_INTERRUPTIBLE);
	if (kanond_balanced(false)) {
		did_short_sleep = true;
		remaining = schedule_timeout(HZ/10);
		finish_wait(&kanond_wait, &wait);
		prepare_to_wait(&kanond_wait, &wait, TASK_INTERRUPTIBLE);
	}
	if (!remaining && kanond_balanced(false)) {
		if (!kthread_should_stop())
			schedule();
	} else {
		if (remaining)
			count_vm_event(KANOND_LOW_WMARK_HIT_QUICKLY);
		else
			count_vm_event(KANOND_HIGH_WMARK_HIT_QUICKLY);
		trace_printk("cannot sleep, remaining: %ld ms\n",
			     jiffies_to_msecs(remaining));
	}
	finish_wait(&kanond_wait, &wait);
	return did_short_sleep;
}

static inline void kanond_update_wmark(void)
{
	static unsigned long kanond_totalram_pages = 0;
	static unsigned long kanond_wmark_high_prev = 0;
	int i, array_size;

	if (!kanond_totalram_pages || kanond_totalram_pages != totalram_pages || need_boot_params) {
		kanond_totalram_pages = totalram_pages;

		if (kanond_wmark_high_force) {
			kanond_wmark_high = kanond_wmark_high_force;
		} else {
			array_size = ARRAY_SIZE(kanond_totalram_tbl);
			for (i = 0; i < array_size; i++) {
				if (totalram_pages <= kanond_totalram_tbl[i]) {
					kanond_wmark_high = kanond_wmark_tbl[i];
					break;
				}
			}
			if (i == array_size)
				kanond_wmark_high = kanond_wmark_tbl[array_size - 1];
		}
		if (need_boot_params) {
			if (jiffies > boot_jiffies)
				need_boot_params = false;
			else
				kanond_wmark_high *= 2;
		}
		kanond_wmark_low = kanond_wmark_high
					- MB_TO_PAGES(KANOND_WMARK_GAP);
		if (kanond_wmark_high_prev != kanond_wmark_high) {
			pr_info("kanond activated with low: %lu MB high: %lu MB min_anon: %lu MB\n",
				M(kanond_wmark_low), M(kanond_wmark_high),
				M(kanond_min_anon));
			kanond_wmark_high_prev = kanond_wmark_high;
		}
	}
}

static int kanond(void *p)
{
	struct shrink_result sr;
	char str_buf[STR_BUF_SIZE];
	unsigned long prev_jiffies;
	unsigned long nr_scanned;
	unsigned long nr_reclaimed;

	boot_jiffies = jiffies + (180 * HZ); /* 3min */

	kanond_update_wmark();
	while (true) {
		if (kanond_try_to_sleep())
			show_meminfo("woken up");
		prev_jiffies = jiffies;
		nr_scanned = 0;
		nr_reclaimed = 0;
		kanond_update_wmark();
		while (!kanond_balanced(true)) {
			shrink_anon_memory(KANOND_NR_RECLAIM, &sr);
			nr_scanned += sr.nr_scanned;
			nr_reclaimed += sr.nr_reclaimed;
		}
		if (!nr_scanned)
			continue;
		snprintf(str_buf, STR_BUF_SIZE,
			 "stopped by %s after r/s %lu/%lu KB (p:%d) %d ms",
			 balanced_reason, K(nr_reclaimed), K(nr_scanned),
			 sr.priority, jiffies_to_msecs(jiffies - prev_jiffies));
		show_meminfo(str_buf);
	}

	return 0;
}

static void __init init_kanond_min_anon(void)
{
	int i, array_size;

	array_size = ARRAY_SIZE(kanond_totalram_tbl);
	for (i = 0; i < array_size; i++) {
		if (totalram_pages <= kanond_totalram_tbl[i]) {
			kanond_min_anon = kanond_min_anon_tbl[i];
			break;
		}
	}
	if (i == array_size)
		kanond_min_anon = kanond_min_anon_tbl[array_size - 1];
}

static int __init kanond_init(void)
{
	struct sched_param param = { .sched_priority = 0 };

	init_kanond_min_anon();
	task_kanond = kthread_run(kanond, NULL, "kanond");
	if (IS_ERR(kanond)) {
		pr_err("Failed to start kanond\n");
		return 0;
	}
	sched_setscheduler(task_kanond, SCHED_IDLE, &param);
	return 0;
}

module_init(kanond_init)
