// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
/*
 * creates two devices with 8 ports. All frames send to one port of first device
 * are received on the corresponding port of the other device and vice versa.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/dmx512/dmx512.h>

/* number of ports to create for both devices */
static unsigned int ports_count = 8;
module_param(ports_count, uint, 0);
MODULE_PARM_DESC(ports_count, "number of ports to create for both dummy devices");

static int dmx512_dummy_send_frame (struct dmx512_port * port, struct dmx512_framequeue_entry * frame)
{
        struct dmx512_port *dst_port = (struct dmx512_port *)dmx512_port_userptr(port);
        if (dst_port)
	    dmx512_received_frame(dst_port, frame);
	return 0;
}

static struct dmx512_device * create_dummy_dmx_device(const char * name)
{
        int ret;
	int i;

        struct dmx512_device *dmx = dmx512_create_device(name ?: "unknown");
	if (IS_ERR(dmx))
		return dmx;

	ret = register_dmx512_device(dmx, 0);
	if (ret < 0) {
		kfree(dmx);
		printk(KERN_ERR "could not register dmx512 dummy device");
		return ERR_PTR(ret);
	}

	for (i = 0; i < ports_count; ++i)
	{
                struct dmx512_port * port = 0;
                char portname[128];
                snprintf(portname, sizeof(portname), "%s-%d", name?: "unknown", i);
                port = dmx512_create_port(dmx, portname, DMX512_CAP_RDM);
		if (port)
		{
                        dmx512_port_set_sendframe(port, dmx512_dummy_send_frame);
			dmx512_add_port(dmx, port);
		}
	}
	return dmx;
}


static struct dmx512_device *g_dmx_devices[2] = {0,0};

static int __init dmx512_dummy_driver_init(void)
{
	struct dmx512_device *dmx0, *dmx1;
        unsigned int i;

	printk(KERN_INFO "loading dmx512 dummy driver\n");
	dmx0 = create_dummy_dmx_device("card0");
	if (IS_ERR(dmx0))
		return PTR_ERR(dmx0);

	dmx1 = create_dummy_dmx_device("card1");
	if (IS_ERR(dmx1)) {
                unregister_dmx512_device(dmx0);
                dmx512_delete_device(dmx0);
		return PTR_ERR(dmx1);
        }
	g_dmx_devices[0] = dmx0;
	g_dmx_devices[1] = dmx1;

	for (i = 0; i < ports_count; ++i)
	{
                dmx512_port_set_userptr(dmx512_port_by_index(dmx0, i), dmx512_port_by_index(dmx1, i));
                dmx512_port_set_userptr(dmx512_port_by_index(dmx1, i), dmx512_port_by_index(dmx0, i));
        }
        
        return 0;
}

static void __exit dmx512_dummy_driver_exit(void)
{
        unregister_dmx512_device(g_dmx_devices[0]);
        unregister_dmx512_device(g_dmx_devices[1]);
        dmx512_delete_device(g_dmx_devices[0]);
        dmx512_delete_device(g_dmx_devices[1]);
	printk(KERN_INFO "unloading dmx512 dummy driver\n");
}

module_init(dmx512_dummy_driver_init);
module_exit(dmx512_dummy_driver_exit);

MODULE_AUTHOR("Michael Stickel");
MODULE_DESCRIPTION("Dummy dmx512 driver module.");
MODULE_VERSION("0.01");
//# MODULE_ALIAS("platform:dmx512-dummy");
MODULE_LICENSE("GPL");
