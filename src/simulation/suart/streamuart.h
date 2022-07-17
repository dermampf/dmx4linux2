#ifndef DEFINED_SUART_H
#define DEFINED_SUART_H

#include "waitqueue.h"

// struct suart_s;
#include "kfifo.h"
#include "streamuart_event.h"
#include "streamuart_struct.h"

struct suart_event_s;

void suart_wakeup       (struct suart_s *u);
int  suart_put_txevents (struct suart_s *u, struct suart_event_s *e, int count);
int  suart_get_txevents (struct suart_s *u, struct suart_event_s *e, int count);
int  suart_peek_txevent (struct suart_s *u, struct suart_event_s *e);
int  suart_put_rxevents (struct suart_s *u, struct suart_event_s *e, int count);
int  suart_get_rxevents (struct suart_s *u, struct suart_event_s *e, int count);
int  suart_txspace      (struct suart_s *u);
int  suart_txavailable  (struct suart_s *u);
int  suart_rxspace      (struct suart_s *u);
int  suart_rxavailable  (struct suart_s *u);

wait_queue_head_t * suart_txwaitqueue (struct suart_s *u);
wait_queue_head_t * suart_rxwaitqueue (struct suart_s *u);

int  suart_init(struct suart_s *u,
                wait_queue_head_t * txwaitqueue,
                wait_queue_head_t * rxwaitqueue
                );

#endif
