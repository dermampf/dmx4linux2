#ifndef DMX512_SUART_H
#define DMX512_SUART_H

struct dmx512suart_port;

struct dmx512suart_port * dmx512suart_create_port
(
 void (*frame_received)(void *userhandle,
			int flags,
                        unsigned char * dmx_data,
                        int dmxsize),
 void * frame_received_handle
 );

struct dmx512suart_port * dmx512suart_create_port_with_index
(
 void (*frame_received)(void *userhandle,
			int flags,
                        unsigned char * dmx_data,
                        int dmxsize),
 void * frame_received_handle,
 int port_index
 );

int dmx512suart_send_frame(struct dmx512suart_port * dmxport,
			   int flags,
                           unsigned char *dmxdata,
                           int count);

void dmx512suart_set_dtr (struct dmx512suart_port * port, int value);

void dmx512suart_delete_port (struct dmx512suart_port * port);

#endif
