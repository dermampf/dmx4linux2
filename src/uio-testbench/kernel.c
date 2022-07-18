#include "kernel.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
//#include <signal.h> // for posix timer functions.
#include <time.h> // for posix timer functions.
#include <ctype.h>

int loglevel = LOGLEVEL_ERR; // everithing from Error
int printk(const char * fmt, ...)
{
	va_list ap;
	int count = 0;
	int i = 0, n;
	char s[1024];

	static int linefeed = 1;
	static int printk_loglevel = -1;

	va_start(ap, fmt);
	n = vsnprintf(s, sizeof(s), fmt, ap);
	va_end(ap);

	if (s[0] == KERN_SOH_ASCII) {
		if (isdigit(s[1])) {
			printk_loglevel = s[1] - '0';
			i += 2;
		}
		else if (s[1] == 'c')
			i += 2;
		else
			printk_loglevel = -1;
	}

	if (printk_loglevel <= loglevel) {
		for (; (i < n) && s[i]; ++i) {
			if (linefeed) {
				struct timespec now;
				if (0==clock_gettime(CLOCK_MONOTONIC, &now))
					count += printf ("[%lu.%09lu]", now.tv_sec, now.tv_nsec);
				else
					count += printf ("[???]");
				linefeed = 0;
			}
			putchar(s[i]);
			++count;
			if (s[i] == '\n')
				linefeed = 1;
		}
	}
	return count;
}

void set_loglevel(const int v)
{
  loglevel = v;
}

struct tasklet_struct * g_tasklet_list = 0;

void tasklet_schedule(struct tasklet_struct *t)
{
	//-- TODO: get global semaphore
	struct tasklet_struct ** l = &g_tasklet_list;
	while (*l) {
		if (*l == t) { // is allready scheduled, so ignore it.
			l = 0;
			break;
		}
		l = &((*l)->next);
	}
	if (l)
		*l = t;
	//-- TODO: put global semaphore
}

void dump_tasklets(struct tasklet_struct * t)
{
	//-- TODO: get global semaphore
	while (t)
	{
		printf ("func:%p data:0x%lX count:%u state:%lu next:%p\n",
			t->func,
			t->data,
			t->count,
			t->state,
			t->next);
		t = t->next;
	}
	//-- TODO: put global semaphore
}

void run_all_tasklets()
{
	while (1)
	{
		//-- TODO: get global semaphore
		struct tasklet_struct * t = g_tasklet_list;
		if (t)
			g_tasklet_list = t->next;
		//-- TODO: put global semaphore
		if (t == 0)
			return;

		t->func(t->data);
	}
}

