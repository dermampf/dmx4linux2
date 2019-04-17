/*
 *  Copyright (C) Nagaraj Krishnamurthy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "uartdmx_tl16c750-platform.h"

static struct dmxuart_tl16c750_platform_data my_device_pdata = {
/*
    .reset_gpio         = 100,
    .power_on_gpio      = 101,
    .power_on		= my_device_power_on,
    .power_off		= my_device_power_off,
    .reset		= my_device_reset
*/
    unsigned long long iobase;
    int irq;
};


static struct platform_device my_device = {
    .name		= "dmx-uart-tl16c750",
    .id			= PLATFORM_DEVID_NONE,
    .dev.platform_data	= &my_device_pdata
};


/* Entry point for loading this module */
static int __init my_driver_init_module(void)
{
        return platform_device_register(&my_device);
}

/* entry point for unloading the module */
static void __exit my_driver_cleanup_module(void)
{
        return platform_device_unregister(&my_device);
}

module_init(my_driver_init_module);
module_exit(my_driver_cleanup_module);
