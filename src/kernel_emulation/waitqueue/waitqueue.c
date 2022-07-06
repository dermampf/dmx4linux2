#include "waitqueue.h"


int __wait_event_interruptible_timeout(wait_queue_head_t * wq_head, unsigned long timeout_jiffies)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += 1000*1000*(timeout_jiffies%1000);
  ts.tv_sec += (timeout_jiffies/1000);
  pthread_mutex_lock(&(wq_head->mutex));
  int ret = pthread_cond_timedwait(&(wq_head->cond), &(wq_head->mutex), &ts);
  if (ret==0)
    {
      struct timespec tsnow;
      clock_gettime(CLOCK_REALTIME, &tsnow);
      unsigned long long ts_ms = ts.tv_nsec/(1000*1000) + 1000*ts.tv_sec;
      unsigned long long tsnow_ms = tsnow.tv_nsec/(1000*1000) + 1000*tsnow.tv_sec;
      unsigned long long d_ms = ts_ms - tsnow_ms;
      ret = d_ms;
    }
  else if (ret == ETIMEDOUT)
    ret = 0;
  else if (ret < 0)
    ret = -ERESTART; // ERESTARTSYS in kernel.
  else
    ret = 0;
  pthread_mutex_unlock(&(wq_head->mutex));
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
