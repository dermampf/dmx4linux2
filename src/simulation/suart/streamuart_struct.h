#ifndef DEFINED_STREAM_UART_STRUCT
#define DEFINED_STREAM_UART_STRUCT

struct suart_s
{
  DECLARE_KFIFO(txfifo, struct suart_event_s, 128);
  DECLARE_KFIFO(rxfifo, struct suart_event_s, 128);
  wait_queue_head_t * txwaitqueue;
  wait_queue_head_t * rxwaitqueue;
};

#endif
