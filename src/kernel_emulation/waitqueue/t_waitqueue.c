#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "waitqueue.h"

const int consumers_count = 2;
const int producers_count = 4;

static volatile int run = 1;

struct arguments
{
  int me;
  wait_queue_head_t *wq;
};

void *producer(void *arg)
{
  struct arguments * a = (struct arguments *)arg;
  while (run)
    {
      sleep(1);
      printf ("ping %d\n", a->me);
      wake_up_interruptible(a->wq);
    }
}

void *consumer(void *arg)
{
  struct arguments * a = (struct arguments *)arg;
  while (run)
    {
      const int ret = wait_event_interruptible_timeout(a->wq, 1, milliseconds_to_jiffies(2000));
      if (ret==0)
        printf ("timeout\n");
      else if (ret > 0)
        printf ("got ping %d\n", a->me);
      else
        printf ("error\n");
    }
}


int main ()
{
  wait_queue_head_t wq;
  init_waitqueue_head(&wq);

  struct arguments producerArgs[producers_count];
  struct arguments consumerArgs[consumers_count];
        
  int i;

  pthread_t consumers[consumers_count];
  for (i = 0; i < sizeof(consumers) / sizeof(consumers[0]); ++ i)
    {
      consumerArgs[i].me = 100+i;
      consumerArgs[i].wq = &wq;
      pthread_create(&consumers[i], NULL, consumer, &consumerArgs[i]);
    }

  pthread_t producers[producers_count];
  for (i = 0; i < sizeof(producers) / sizeof(producers[0]); ++ i)
    {
      producerArgs[i].me = i;
      producerArgs[i].wq = &wq;
      usleep(1000*1000/producers_count);
      pthread_create(&producers[i], NULL, producer, &producerArgs[i]);
    }



  sleep(4);
  run = 0;
  for (i = 0; i < sizeof(producers) / sizeof(producers[0]); ++ i)
    {
      pthread_join(producers[i], NULL);
    }
  for (i = 0; i < sizeof(consumers) / sizeof(consumers[0]); ++ i)
    {
      pthread_join(consumers[i], NULL);
    }

  
  return 0;
}
