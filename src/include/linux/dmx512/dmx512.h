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

#define MAX_DMXPORT_NAME 128 /* how long can a dmx name be? */

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

extern int register_dmx512_device(struct dmx512_device * dev, void * data);
extern int unregister_dmx512_device(struct dmx512_device * dev);
extern int devm_register_dmx512_device(struct device * dev, struct dmx512_device * dmx, void * data);

#define DMX512_CAP_RDM (1<<0)
struct dmx512_port {
	struct list_head device_item; /* port in dmx512-device */
        struct list_head portlist_item; /* port in the global ports list */

	struct dmx512_device *device;
	char     name[MAX_DMXPORT_NAME];
	uint64_t capabilities;

	int (*send_frame) (struct dmx512_port * port, struct dmx512_framequeue_entry * frame);
    // struct dmx512_framequeue rxframequeue;
    // struct dmx512_framequeue txframequeue;
};

extern int dmx512_add_port(struct dmx512_device * dev, struct dmx512_port *port);
extern int dmx512_remove_port(struct dmx512_port *port);
extern struct dmx512_port * dmx512_port_by_index(struct dmx512_device * device, const int index);
extern int dmx512_port_index(struct dmx512_port * port);

extern int dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame);

#endif
