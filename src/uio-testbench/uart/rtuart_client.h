#ifndef DEFINED_RTUART_CLIENT
#define DEFINED_RTUART_CLIENT

struct rtuart;
struct rtuart_client_callbacks
{
  int (*transmitter_has_space)(struct rtuart *, const int space_available);
  int (*transmitter_empty)(struct rtuart *);
  int (*receiver_data_available)(struct rtuart *, const int count);

  int (*modem_input_changed)(struct rtuart *, const unsigned long values, const unsigned long changemask);
  int (*line_status_event)(struct rtuart *, const unsigned long eventmask);
};

struct rtuart_client
{
  const struct rtuart_client_callbacks * callbacks;
};

#endif
