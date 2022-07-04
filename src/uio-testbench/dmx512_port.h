// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_DMX512_PORT
#define DEFINED_DMX512_PORT

#include <linux/dmx512/dmx512frame.h>

#include "kernel.h"

#include <stdint.h>

//---- kernel dmx512 header substution --------
typedef struct dmx512_framequeue_entry
{
    struct dmx512_framequeue_entry * head;
    struct dmx512frame  frame;
} dmx512_framequeue_entry_t;

enum { MAX_DMXPORT_NAME = 100 };
enum { DMX512_CAP_RDM = (1<<0) };
struct dmx512_port {
	char     name[MAX_DMXPORT_NAME];
	uint64_t capabilities;
	int (*send_frame) (struct dmx512_port * port, struct dmx512_framequeue_entry * frame);
    // struct dmx512_framequeue rxframequeue;
    // struct dmx512_framequeue txframequeue;
};

struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_port *port);
void dmx512_put_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame);
int  dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame);
int dmx512_send_frame (struct dmx512_port * port, struct dmx512_framequeue_entry * frame);

//---- kernel dmx512 header substution END --------
#endif
