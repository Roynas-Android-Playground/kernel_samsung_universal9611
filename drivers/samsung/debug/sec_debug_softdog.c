/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sec_debug.h>
#include <linux/stacktrace.h>
#include <linux/debug-snapshot.h>
#include <asm/stack_pointer.h>
#include <asm/stacktrace.h>
#include <linux/sched/debug.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "../../../kernel/sched/sched.h"
#include <linux/sched/signal.h>

static call_single_data_t csd;
static struct task_struct *suspect;

static inline char get_state(struct task_struct *p)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'X', 'Z', 'P', 'x', 'K', 'W', 'I', 'N'};
	unsigned char idx = 0;
	unsigned long state;

	if (!p)
		return 0;

	state = (p->state | p->exit_state) & (TASK_STATE_MAX - 1);

	while (state) {
		idx++;
		state >>= 1;
	}

	return state_array[idx];
}

static void show_callstack(void *dummy)
{
#ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
	show_stack_auto_comment(NULL, NULL);
#else
	show_stack(NULL, NULL);
#endif
}

static struct task_struct *secdbg_softdog_find_key_suspect(void)
{
	struct task_struct *c, *g;

	for_each_process(g) {
		if (g->pid == 1)
			suspect = g;
		else if (!strcmp(g->comm, "system_server")) {
			suspect = g;
			for_each_thread(g, c) {
				if (!strcmp(c->comm, "watchdog")) {
					suspect = c;
					goto out;
				}
			}
			goto out;
		}
	}

out:
	return suspect;
}

void secdbg_softdog_show_info(void)
{
	struct task_struct *p = NULL;

	p = secdbg_softdog_find_key_suspect();

	if (p) {
		pr_auto(ASL5, "[SOFTDOG] %s:%d %c(%d) exec:%lluns\n",
			p->comm, p->pid, get_state(p), (int)p->state, p->se.exec_start);

		if (task_running(task_rq(p), p)) {
			csd.flags = 0;
			csd.func = show_callstack;
			csd.info = 0;
			smp_call_function_single_async(task_cpu(p), &csd);
		} else {
#ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
			show_stack_auto_comment(p, NULL);
#else
			show_stack(p, NULL);
#endif
		}
	} else {
		pr_auto(ASL5, "[SOFTDOG] Init task not exist!\n");
	}
}
