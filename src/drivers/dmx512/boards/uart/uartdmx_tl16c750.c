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

struct dmxuart_tl16c750_driver_data
{
	struct platform_device platform_device;

	// any members nessesary for this device
	// ...
	struct tl16c750_regs *regs;
	int irq;
	int num_ports;
	struct dmx512_device dev;
	struct dmx512_port   port[1]; /* port[num_ports] */
};


static int dmxuart_tl16c750_send_frame (struct dmx512_port * port, struct dmx512_framequeue_entry * frame)
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

static int dmxuart_tl16c750_pm_suspend(struct device *dev)
{
	// struct my_driver_data* driver_data = dev_get_drvdata(dev);
	printk(KERN_ALERT " %s\n", __FUNCTION__);
	/* Suspend the device here*/
	return 0;
}


static int dmxuart_tl16c750_pm_resume(struct device *dev)
{
	// struct my_driver_data* driver_data = dev_get_drvdata(dev);
	printk(KERN_ALERT " %s\n", __FUNCTION__);
	/* Resume the device here*/
	return 0;
}

static int dmxuart_tl16c750_driver_probe(struct platform_device *pdev)
{
	int i;
	int ret;
	const int num_ports = 1;
	// struct my_device_platform_data *device_pdata = dev_get_platdata(&pdev->dev);
	struct dmxuart_tl16c750_driver_data* driver_data;

	printk(KERN_INFO "probing loading dmx512 dummy driver\n");

	driver_data = kzalloc(sizeof(struct dmxuart_tl16c750_driver_data) + (num_ports-1)*sizeof(struct dmx512_port), GFP_KERNEL);
	if (driver_data == NULL)
		return -ENOMEM;
	driver_data->dev.name = "uartdmx-tl16c750";
	driver_data->dev.parent = &prev->dev;
	driver_data->dev.owner = THIS_MODULE;
	driver_data->num_ports = num_ports;
	ret = register_dmx512_device(&driver_data->dev, 0);
	if (ret < 0) {
		kfree(dmx);
                printk(KERN_ERR "could not register %s device", driver_data->dev.name);
                return ret;
	}

	for (i = 0; i < num_ports; ++i)
	{
		struct dmx512_port * port = &driver_data->port[i];
		port->device = &driver_data->dev;
		snprintf(port->name, sizeof(port->name), "%s-%d", driver_data->dev.name, i);
		port->capabilities = DMX512_CAP_RDM;
		port->send_frame = dmxuart_tl16c750_send_frame;
		dmx512_add_port(&driver_data->dev, port);
	}

#if 0
	/* Power on the device. */
	if (my_device_pdata->power_on) {
		my_device_pdata->power_on(my_device_pdata);
	}

	/* wait for some time before we do the reset */
	mdelay(5);

	/* Reset the device. */
	if (my_device_pdata->reset) {
		my_device_pdata->reset(my_device_pdata);
	}
#endif
	driver_data->platform_device.name = driver_data->dev.name;
	driver_data->platform_device.id = PLATFORM_DEVID_NONE;
	driver_data->platform_device.dev.platform_data    = driver_data;
	platform_device_register(&driver_data->platform_device); /* handle result ? */

	/* Set this driver data in platform device structure */
	platform_set_drvdata(pdev, driver_data);

	return 0;
}


static int dmxuart_tl16c750_driver_remove(struct platform_device *pdev)
{
	// struct my_device_platform_data *my_device_pdata = dev_get_platdata(&pdev->dev);;
    struct dmxuart_tl16c750_driver_data *driver_data = platform_get_drvdata(pdev);

    if (!driver_data)
	return -EINVAL;

    printk(KERN_INFO "unloading %s driver\n", driver_data->dev.name);

    unregister_dmx512_device(&driver_data->dev);

    // kfree(driver_data); -- is this done by the platform drivers?
/*
    // Power Off the device
    if (my_device_pdata->power_off) {
        my_device_pdata->power_off(my_device_pdata);
    }
*/
    return 0;
}


static const struct dev_pm_ops dmxuart_tl16c750_pm_ops = {
    .suspend	= dmxuart_tl16c750_pm_suspend,
    .resume	= dmxuart_tl16c750_pm_resume,
};

static struct platform_driver dmxuart_tl16c750_driver = {
    .probe      = dmxuart_tl16c750_driver_probe,
    .remove     = dmxuart_tl16c750_driver_remove,
    .driver     = {
        .name     = "dmx-uart-tl16c750",
        .owner    = THIS_MODULE,
	.pm       = &dmxuart_tl16c750_pm_ops,
    },
};

static int __init dmxuart_tl16c750_driver_init(void)
{
    return platform_driver_register(&dmxuart_tl16c750_driver);
}

static void __exit dmxuart_tl16c750_driver_exit(void)
{
    platform_driver_unregister(&dmxuart_tl16c750_driver);
}

module_init(dmxuart_tl16c750_driver_init);
module_exit(dmxuart_tl16c750_driver_exit);

MODULE_AUTHOR("Michael Stickel");
MODULE_DESCRIPTION("TL16c750 base uart dmx driver.");
MODULE_VERSION("0.01");
MODULE_ALIAS("platform:dmx-uart-tl16c750");
MODULE_LICENSE("GPL");
