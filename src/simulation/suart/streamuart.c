#include "streamuart.h"

#include "streamuart_event.h"
#include "kfifo.h"
#include "streamuart_struct.h"


void suart_wakeup (struct suart_s *u)
{
  wake_up_interruptible(u->txwaitqueue);
}

int suart_put_txevents (struct suart_s *u, struct suart_event_s *e, int count)
{
  int ret = kfifo_in(&u->txfifo, e, count);
  suart_wakeup (u);
}

int suart_get_txevents (struct suart_s *u, struct suart_event_s *e, int count)
{
  return kfifo_out(&u->txfifo, e, count);
}

int suart_peek_txevent (struct suart_s *u, struct suart_event_s *e)
{
  return kfifo_peek(&u->txfifo, e);
}

int suart_put_rxevents (struct suart_s *u, struct suart_event_s *e, int count)
{
  int ret = kfifo_in(&u->rxfifo, e, count);
  wake_up_interruptible(u->rxwaitqueue);
}

int suart_get_rxevents (struct suart_s *u, struct suart_event_s *e, int count)
{
  return kfifo_out(&u->rxfifo, e, count);
}

int suart_txspace    (struct suart_s *u)
{
  return kfifo_avail(&u->txfifo);
}

int suart_txavailable(struct suart_s *u)
{
  return kfifo_len(&u->txfifo);
}

int suart_rxspace (struct suart_s *u)
{
  return kfifo_avail(&u->rxfifo);
}

int suart_rxavailable(struct suart_s *u)
{
  return kfifo_len(&u->rxfifo);
}

wait_queue_head_t * suart_txwaitqueue (struct suart_s *u)
{
  return u->txwaitqueue;
}

wait_queue_head_t * suart_rxwaitqueue (struct suart_s *u)
{
  return u->rxwaitqueue;
}


int suart_init(struct suart_s *u,
               wait_queue_head_t * txwaitqueue,
               wait_queue_head_t * rxwaitqueue
               )
{
  INIT_KFIFO(u->txfifo);
  INIT_KFIFO(u->rxfifo);
  u->txwaitqueue = txwaitqueue;
  u->rxwaitqueue = rxwaitqueue;
  return 0;
}
