/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DRIVER_DMX512_H
#define DRIVER_DMX512_H

/* Siehe devm_drm_panel_bridge_add in ~/Downloads/linux-5.1-rc4/drivers/gpu/drm/bridge/panel.c */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
//#include <linux/poll.h>
#include <linux/wait.h>

#include <linux/dmx512/dmx512framequeue.h>

struct dmx512_port;
struct dmx512_frame;
struct dmx512_device;

#define MAX_DMXPORT_NAME 128 /* how long can a dmx name be? */

extern struct dmx512_device * dmx512_create_device(const char * name);
extern void dmx512_delete_device(struct dmx512_device *device);

extern int register_dmx512_device(struct dmx512_device * dev, void * data);
extern int unregister_dmx512_device(struct dmx512_device * dev);
extern int devm_register_dmx512_device(struct device * dev, struct dmx512_device * dmx, void * data);

extern const char * dmx512_device_name(struct dmx512_device * dmx);

#define DMX512_CAP_RDM (1<<0)


extern struct dmx512_port * dmx512_create_port(struct dmx512_device *dmx,
                                               const char * portname,
                                               const uint64_t capabilities);
extern void dmx512_delete_port(struct dmx512_port * port);
extern void dmx512_port_set_sendframe(struct dmx512_port *port,
                                      int (*callback_fn) (struct dmx512_port *,
                                                          struct dmx512_framequeue_entry *));


extern int dmx512_add_port(struct dmx512_device * dev, struct dmx512_port *port);
extern int dmx512_remove_port(struct dmx512_port *port);
extern struct dmx512_port * dmx512_port_by_index(struct dmx512_device * device, const int index);
extern int dmx512_port_index(struct dmx512_port * port);

extern void dmx512_port_set_userptr(struct dmx512_port * port, void * userptr);
extern void * dmx512_port_userptr(struct dmx512_port * port);

extern const char * dmx512_port_name(struct dmx512_port * port);

extern int dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame);

extern void dmx512_put_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame);
extern struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_port *port);

#endif
