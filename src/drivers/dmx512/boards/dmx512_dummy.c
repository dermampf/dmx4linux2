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
	/*
	 * Send the frame to the same port of the receive queue of the other device.
	 * No need to change the frame, as the data is valid on the recive path.
	 * The timestamp difference is not worth it.
	 */
	struct dmx512_port * dst_port = dmx512_port_by_index(port->device, dmx512_port_index(port) ^ 1);
	if (dst_port)
	    dmx512_received_frame(dst_port, frame);
	return 0;
}


static struct dmx512_device *dmx = 0;
static int __init dmx512_dummy_driver_init(void)
{
        int ret;
	int i;

	printk(KERN_INFO "loading dmx512 dummy driver\n");

        dmx = kzalloc(sizeof(*dmx), GFP_KERNEL);
        if (dmx == NULL)
                return -ENOMEM;

	dmx->name = "dummy";
	dmx->parent = 0;
	dmx->owner = THIS_MODULE;
	ret = register_dmx512_device(dmx, 0);
	if (ret < 0) {
		kfree(dmx);
                printk(KERN_ERR "could not register dmx512 dummy device");
                return ret;
	}

	for (i = 0; i < ports_count; ++i)
	{
		struct dmx512_port * port = kzalloc(sizeof(*port), GFP_KERNEL);
		if (port)
		{
			port->device = dmx;
			snprintf(port->name, sizeof(port->name), "%s-%d", dmx->name, i);
			port->capabilities = DMX512_CAP_RDM;
			port->send_frame = dmx512_dummy_send_frame;
			dmx512_add_port(dmx, port);
		}
	}
        return ret;
}

static void __exit dmx512_dummy_driver_exit(void)
{
    if (!dmx)
	return;
    unregister_dmx512_device(dmx);
    kfree(dmx);
    printk(KERN_INFO "unloading dmx512 dummy driver\n");
}

module_init(dmx512_dummy_driver_init);
module_exit(dmx512_dummy_driver_exit);

MODULE_AUTHOR("Michael Stickel");
MODULE_DESCRIPTION("Dummy dmx512 driver module.");
MODULE_VERSION("0.01");
//# MODULE_ALIAS("platform:dmx512-dummy");
MODULE_LICENSE("GPL");
