#ifndef DEFINED_RTUART_DMX512_CLIENT
#define DEFINED_RTUART_DMX512_CLIENT

struct rtuart;
struct dmx512_port;
struct dmx512_port * dmx512_rtuart_client_create(struct rtuart * uart);
int  dmx512_rtuart_client_is_busy(struct rtuart * uart);

#endif
