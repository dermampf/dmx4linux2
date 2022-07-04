/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DRIVER_DMX512_PRIV_H
#define DRIVER_DMX512_PRIV_H

#include <linux/dmx512/dmx512.h>

extern struct dmx512_framequeue free_framequeue;


struct dmx512_device {
	struct list_head devicelist_item;
	const char    * name;
	struct device * parent;
	struct module * owner;
	void          * data; /* filled by the register function with it's data parameter */
	struct list_head ports;
	struct miscdevice miscdev;
        struct dmx512_framequeue rxframequeue;
        wait_queue_head_t rxwait_queue;
};

struct dmx512_port {
	struct list_head device_item; /* port in dmx512-device */
        struct list_head portlist_item; /* port in the global ports list */

	struct dmx512_device *device;
	char     name[MAX_DMXPORT_NAME];
	uint64_t capabilities;

	int (*send_frame) (struct dmx512_port * port, struct dmx512_framequeue_entry * frame);
	int (*transmitter_has_space) (struct dmx512_port * port);
    // struct dmx512_framequeue rxframequeue;
    // struct dmx512_framequeue txframequeue;
        void * userptr;
};

#endif
