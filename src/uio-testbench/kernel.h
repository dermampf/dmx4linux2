// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_KERNEL
#define DEFINED_KERNEL

typedef unsigned long  u32;
typedef unsigned short u16;
typedef unsigned char  u8;

#include <unistd.h>
#include "kern_levels.h"

int printk(const char * fmt, ...);
void set_loglevel(const int);

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE, MEMBER)  __compiler_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({              \
        void *__mptr = (void *)(ptr);                   \
        ((type *)((unsigned long long)__mptr - offsetof(type, member))); })



// some tasklet wrapper stuff
typedef unsigned int atomic_t;
struct tasklet_struct
{
    struct tasklet_struct *next;  /* The next tasklet in line for scheduling */
    unsigned long state;          /* TASKLET_STATE_SCHED or TASKLET_STATE_RUN */
    atomic_t count;               /* Responsible for the tasklet being activated or not */
    void (*func)(unsigned long);  /* The main function of the tasklet */
    unsigned long data;           /* The parameter func is started with */
};

static inline void tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data)
{
    t->next = 0;
    t->state = 0;
    t->count = 0;
    t->func = func;
    t->data = data;
}

#define DECLARE_TASKLET(name, func, data)				\
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }

#define DECLARE_TASKLET_DISABLED(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(1), func, data }

void tasklet_schedule(struct tasklet_struct *t);           /* with normal priority */
static inline void tasklet_hi_schedule(struct tasklet_struct *t) { tasklet_schedule(t); }
static inline void tasklet_hi_schedule_first(struct tasklet_struct *t) { tasklet_schedule(t); }

void dump_tasklets(struct tasklet_struct * t);
void run_all_tasklets();

#endif
