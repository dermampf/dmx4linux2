#ifndef WAITQUEUE_H
#define WAITQUEUE_H

#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


// We assume jiffies are in milliseconds
#define milliseconds_to_jiffies(ms)      (ms)
#define jiffies_to_milliseconds(jiffies) (jiffies)


struct wait_queue_head
{
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
};
typedef struct wait_queue_head wait_queue_head_t;



/**
 * wait_event_interruptible_timeout - sleep until a condition gets true or a timeout elapses
 * @wq_head: the waitqueue to wait on
 * @condition: a C expression for the event to wait for
 * @timeout: timeout, in jiffies
 *
 * The process is put to sleep (TASK_INTERRUPTIBLE) until the
 * @condition evaluates to true or a signal is received.
 * The @condition is checked each time the waitqueue @wq_head is woken up.
 *
 * wake_up() has to be called after changing any variable that could
 * change the result of the wait condition.
 *
 * Returns:
 * 0 if the @condition evaluated to %false after the @timeout elapsed,
 * 1 if the @condition evaluated to %true after the @timeout elapsed,
 * the remaining jiffies (at least 1) if the @condition evaluated
 * to %true before the @timeout elapsed, or -%ERESTARTSYS if it was
 * interrupted by a signal.
 */
int __wait_event_interruptible_timeout(wait_queue_head_t * wq_head, unsigned long timeout_jiffies);

/*
 * Returns:
 * 0 : @cond==false && @timeout           !@cond && @timeout  => 0
 * 1 : @cond==true  && @timeout           @cond && @timeout   => 1
 * @remain : @cond==true !@timeout        @cond && !@timeout  => @remain
 *
 * Returns:
 * 0 : @cond==false && @timeout           !@cond && !__ret   => 0
 * 1 : @cond==true  && @timeout           @cond  && !__ret   => 1
 * @remain : @cond==true !@timeout        @cond  &&  __ret>0 => __ret
 *
 * __ret := 0, @remain   with: (remain>0)
 *
 * @timeout := !__ret  ( __ret=0)
 *
 * 0 if the @condition evaluated to %false after the @timeout elapsed,
 * 1 if the @condition evaluated to %true after the @timeout elapsed,
 * the remaining jiffies (at least 1) if the @condition evaluated
 * to %true before the @timeout elapsed, or -%ERESTARTSYS if it was
 * interrupted by a signal.
 */
#define ___wait_cond_timeout(condition)                                 \
  ({                                                                    \
    char __cond = (condition);                                          \
    if (__cond && !__ret)                                               \
      __ret = 1;                                                        \
    __cond || !__ret;                                                   \
  })
/*
 * __cond  __ret    ___wait_cond_timeout
 *    0      0        1
 *    0      >=1      0
 *    1      0        1                       __ret'=1
 *    1      >=1      1 
 */

#define wait_event_interruptible_timeout(wq_head, condition, timeout_jiffies) \
  ({                                                                    \
    int __ret;                                                          \
    do {                                                                \
      __ret = __wait_event_interruptible_timeout(wq_head, timeout_jiffies); \
    }                                                                   \
    while (!___wait_cond_timeout(condition));   \
    __ret;                                      \
  })

void wake_up_interruptible(wait_queue_head_t * w);
int init_waitqueue_head(wait_queue_head_t * w);

#endif
