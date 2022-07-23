#include "waitqueue.h"

#include <sys/types.h>
#include <string.h>

// put to #include "timespec.h"
static void timespec_add_milliseconds(struct timespec *ts, unsigned long milliseconds)
{
  ts->tv_sec += milliseconds/1000;
  if (milliseconds % 1000)
    {
      ts->tv_nsec += (milliseconds % 1000) * 1000000LL;
      ts->tv_sec += ts->tv_nsec / 1000000000LL;
      ts->tv_nsec %= 1000000000LL;
    }
}

static void timespec_sub(struct timespec *ts, struct timespec *subtractor)
{
  ts->tv_nsec -= subtractor->tv_nsec;
  // can not be less than -999999999LL
  ts->tv_sec  -= subtractor->tv_sec;
  while (ts->tv_nsec < 0)
    {
      ts->tv_sec -= 1;
      ts->tv_nsec += 1000000000L;
    }
}
unsigned long timespec_to_milliseconds(struct timespec *ts)
{
  return ts->tv_nsec/(1000000L) + 1000*ts->tv_sec;
}


int __wait_event_interruptible_timeout(wait_queue_head_t * wq_head, unsigned long timeout_jiffies)
{
  struct timespec ts_then;

  clock_gettime(CLOCK_REALTIME, &ts_then);
  timespec_add_milliseconds(&ts_then, jiffies_to_milliseconds(timeout_jiffies));
  pthread_mutex_lock(&(wq_head->mutex));
  int ret = pthread_cond_timedwait(&(wq_head->cond), &(wq_head->mutex), &ts_then);
  // if (ret = EAGAIN) ret = pthread_cond_timedwait(&(wq_head->cond), &(wq_head->mutex), &ts_then);
  pthread_mutex_unlock(&(wq_head->mutex));
  if (ret==0)
    {
      struct timespec tsnow;
      clock_gettime(CLOCK_REALTIME, &tsnow);
      timespec_sub(&ts_then, &tsnow);
      ret = timespec_to_milliseconds(&ts_then);
    }
  else if (ret == ETIMEDOUT)
    ret = 0;
  else if (ret < 0)
    ret = -ERESTART; // ERESTARTSYS in kernel.
  else
    ret = -ret;
  return ret;
}

void wake_up_interruptible(wait_queue_head_t * w)
{
  pthread_cond_broadcast(&(w->cond));
}

int init_waitqueue_head(wait_queue_head_t * w)
{
  wait_queue_head_t _w = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
  *w = _w;
}

#ifdef TEST_WAITQUEUE
int main ()
{

  struct timespec a, b, c;

  a.tv_sec=0;
  a.tv_nsec=0;
  printf ("%ld.%ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 1);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 1000);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 1000);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 1);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 2100);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  timespec_add_milliseconds(&a, 950);
  printf ("%ld.%09ld\n", a.tv_sec, a.tv_nsec);

  b.tv_sec=1; b.tv_nsec=0;
  c.tv_sec=0; c.tv_nsec=1;
  timespec_sub(&b, &c);
  printf ("%ld.%09ld\n", b.tv_sec, b.tv_nsec);

  printf ("%lu\n", timespec_to_milliseconds(&b));


  return 0;
}
#endif
