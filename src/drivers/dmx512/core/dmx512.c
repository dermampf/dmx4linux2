/*
 * The dmx512 core infrastructure. Contains all dmx512 registration and deregistration functions,
 * as well as all dmx512 core utilities.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/workqueue.h>



#include <linux/dmx512/dmx512_priv.h>
#include <linux/dmx512/dmx512.h>

DEFINE_SPINLOCK(dmx512_lock);
#define DMX512_LOCK spin_lock_irqsave(&dmx512_lock, flags)
#define DMX512_UNLOCK spin_unlock_irqrestore(&dmx512_lock, flags)
/* with the while statement we can catch breaks and unlock safely */
#define DMX512_LOCKED(code) { unsigned long flags; DMX512_LOCK; do { code; } while(0); DMX512_UNLOCK; }

LIST_HEAD(dmx512_devices);
LIST_HEAD(dmx512_ports);

struct dmx512_framequeue free_framequeue;

static int _dmx512_add_port(struct dmx512_device * dev, struct dmx512_port * port)
{
    port->device = dev;
    list_add_tail(&port->device_item, &dev->ports);
    list_add_tail(&port->portlist_item, &dmx512_ports);

    printk("dmx512: add port %s to device %s\n", port->name, dev->name);

    return 0;
}

static int _dmx512_remove_port(struct dmx512_port * port)
{
    printk("dmx512: remove port %s\n", port->name);

    list_del(&port->device_item);
    list_del(&port->portlist_item);
    port->device = 0;
    return 0;
}

#define miscdev_to_dmx512device(d) container_of(d, struct dmx512_device, miscdev)


static int dmx512_device_open (struct inode *inode, struct file * filp)
{
    struct dmx512_device *dmx = miscdev_to_dmx512device(filp->private_data);
    if (!dmx)
	return -ENODEV;
    filp->private_data = dmx;

    if (!try_module_get(dmx->owner))
	return -ENODEV;

    printk("open dmx512 device %s\n", dmx->name);

    return 0;
}

static int dmx512_device_release (struct inode * inode, struct file * filp)
{
    struct dmx512_device *dmx = (struct dmx512_device *)filp->private_data;
    if (!dmx)
	return -ENODEV;

    printk("close dmx512 device %s\n", dmx->name);

    module_put(dmx->owner);

    return 0;
}

static ssize_t dmx512_device_read (struct file * filp, char __user * buf, size_t size, loff_t * off)
{
    struct dmx512_device *dmx = (struct dmx512_device *)filp->private_data;
    struct dmx512_framequeue_entry * e;

    if (size < sizeof(struct dmx512frame))
        return -EINVAL;

    if (dmx512_framequeue_isempty(&dmx->rxframequeue))
    {
	    if (filp->f_flags & O_NONBLOCK)
		    return -EAGAIN;
	    wait_event_interruptible (dmx->rxwait_queue, 0==dmx512_framequeue_isempty(&dmx->rxframequeue));
    }
    e = dmx512_framequeue_get (&dmx->rxframequeue);
    if (e)
    {
        const int stat = copy_to_user (buf, &(e->frame), sizeof(struct dmx512frame));
        dmx512_framequeue_put(&free_framequeue, e);
        if (stat==0)
          return sizeof(struct dmx512frame);
    }
    return 0;
}

static ssize_t dmx512_device_write (struct file * filp, const char __user * buf, size_t size, loff_t * off)
{
	ssize_t err = -ENODEV;
	struct dmx512_device *dmx = (struct dmx512_device *)filp->private_data;
	if (dmx)
	{
		// do not remove the entry from the queue, just get the port-number from the top entry.
		// then find the port for that entry and if the port is not ready then:
		// either return -EAGAIN or -EBUSY if the device is opened with O_NONBLOCK
		// or wait until the port becomes ready again.
		// then fetch the entry from the queue and send it to the port.
		struct dmx512_framequeue_entry * e = dmx512_framequeue_get (&free_framequeue);
		if (!e)
			return -ENOMEM;

		err = -EINVAL;
		if ((size >= sizeof(struct dmx512frame)) && !copy_from_user (&e->frame, buf, sizeof(struct dmx512frame)))
		{
			struct dmx512_port * p = dmx512_port_by_index(dmx, e->frame.port);
			if (p)
			{
#if 0
				if (!(filp->f_flags & O_NONBLOCK) && p->transmitter_has_space && !p->transmitter_has_space(p))
				{
					wait_event_interruptible (p->txwait_queue, p->transmitter_has_space(p));
				}
#endif
				if ((p->transmitter_has_space==0) || p->transmitter_has_space(p))
				{
					// dmx512_framequeue_put(&dmx->txframequeue, e);
					p->send_frame(p, e);
					return sizeof(struct dmx512frame);
				}
				err = -EAGAIN; // or -EBUSY
			}
		}
		dmx512_framequeue_put(&free_framequeue, e);
	}
	return err;
}


static unsigned int dmx512_device_poll (struct file *file, poll_table *wait)
{
	struct dmx512_device *dmx = (struct dmx512_device *)file->private_data;
	if (dmx)
	{
		unsigned int ret;
		ret = 0;
		poll_wait(file, &dmx->rxwait_queue, wait);

		if (!dmx512_framequeue_isempty(&dmx->rxframequeue))
			ret |= POLLIN | POLLRDNORM;
#if 0
		if(dmx512_framequeue_isempty(&dmx->txframequeue)) // buffer not full. write wont block
			ret |= POLLOUT | POLLWRNORM;
#endif
		return ret;
	}
	return 0;
}

static long dmx512_device_ioctl (struct file * filp,
				 unsigned int command,
				 unsigned long arg)
{
	return 0;
}

static const struct file_operations dmx512_device_fops = {
    .open    = dmx512_device_open,
    .release = dmx512_device_release,

    .read  = dmx512_device_read,
    .write = dmx512_device_write,
    .poll  = dmx512_device_poll,

    .unlocked_ioctl = dmx512_device_ioctl,
    // long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);

    .compat_ioctl = dmx512_device_ioctl,
    // long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
};

static int _register_dmx512_device(struct dmx512_device * dev, void * data)
{
    int err;

    dev->data = data;

    INIT_LIST_HEAD(&dev->ports);
    init_waitqueue_head(&dev->rxwait_queue);
    dmx512_framequeue_init(&dev->rxframequeue);

    list_add(&dev->devicelist_item, &dmx512_devices);

    dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    dev->miscdev.name = kasprintf(GFP_KERNEL, "dmx-%s", dev->name);
    dev->miscdev.fops = &dmx512_device_fops;
    err = misc_register(&dev->miscdev);
    return err;
}

static int _unregister_dmx512_device(struct dmx512_device * dev)
{
    /* remove all ports */
    struct dmx512_port *port, *tmp;
    wake_up(&dev->rxwait_queue);
    misc_deregister(&dev->miscdev);
    list_for_each_entry_safe(port, tmp, &dev->ports, device_item) {
	_dmx512_remove_port(port);
	kfree(port);
    }
    list_del(&dev->devicelist_item);
    dmx512_framequeue_cleanup(&dev->rxframequeue);
    return 0;
}

//static int _devm_deregister_dmx512_device(struct device * dev);
static int _devm_register_dmx512_device(struct device * dev, struct dmx512_device * dmx, void * data)
{
    // int ret;
    // ret = _register_dmx512_device(dmx, data);
    return -1;
}


static int _dmx512_port_index(struct dmx512_port * port)
{
    int index = 0;
    struct dmx512_port *p, *tmp;
    if (port == 0)
	return -1;
    list_for_each_entry_safe(p, tmp, &port->device->ports, device_item) {
	if (port == p)
	    return index;
	index++;
    }
    return -1;
}

/*
 * _dmx512_port_by_index(device, 1) returns the second port.
 */
static struct dmx512_port * _dmx512_port_by_index(struct dmx512_device * device, const int index)
{
    int i = 0;
    struct dmx512_port *port, *tmp;
    if (device == 0)
	return 0;
    list_for_each_entry_safe(port, tmp, &device->ports, device_item) {
	if (index == i)
	    return port;
	i++;
    }
    return 0;
}


/*---- public functions ----*/

struct dmx512_device * dmx512_create_device(const char * name)
{
        struct dmx512_device *dmx = kzalloc(sizeof(*dmx), GFP_KERNEL);
	if (dmx == NULL)
		return ERR_PTR(-ENOMEM);

	dmx->name = kstrdup(name ?: "unknown", GFP_KERNEL);
	dmx->parent = 0;
	dmx->owner = THIS_MODULE;
        return dmx;
}
EXPORT_SYMBOL(dmx512_create_device);

void dmx512_delete_device(struct dmx512_device *device)
{
	if (device)
        {
		// unregister_dmx512_device(device);
                // loop over all ports:port and call dmx512_delete_port(port);
		kfree(device);
        }
}
EXPORT_SYMBOL(dmx512_delete_device);



int register_dmx512_device(struct dmx512_device * dev, void * data)
{
    int ret;
    DMX512_LOCKED(ret = _register_dmx512_device(dev, data));
    return ret;
}
EXPORT_SYMBOL(register_dmx512_device);

int unregister_dmx512_device(struct dmx512_device * dev)
{
    int ret = 0;
    if (dev) {
            DMX512_LOCKED(ret = _unregister_dmx512_device(dev));
    }
    return ret;
}
EXPORT_SYMBOL(unregister_dmx512_device);

int devm_register_dmx512_device(struct device * dev, struct dmx512_device * dmx, void * data)
{
    int ret;
    DMX512_LOCKED(ret = _devm_register_dmx512_device(dev, dmx, data));
    return 0;
}
EXPORT_SYMBOL(devm_register_dmx512_device);


const char * dmx512_device_name(struct dmx512_device * dmx)
{
        return dmx ? dmx->name : 0;
}
EXPORT_SYMBOL(dmx512_device_name);

/* add port to device and to dmx512_ports
 *
 * snprintf(port->name, sizeof(port->name), "name-%d", number);
 * port->capabilities = ....;
 * port->send_frame = ...;
 * port->transmitter_has_space = ...;
 * dmx512_add_port(device, port);
 */

struct dmx512_port * dmx512_create_port(struct dmx512_device *dmx,
                                        const char * portname,
                                        const uint64_t capabilities)
{
        struct dmx512_port * port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (port == NULL)
		return ERR_PTR(-ENOMEM);

        port->device = dmx;
        strcpy(port->name, portname);
        port->capabilities = capabilities;
        port->send_frame = 0;
        return port;
}
EXPORT_SYMBOL(dmx512_create_port);

void dmx512_delete_port(struct dmx512_port * port)
{
        if (port) {
                dmx512_remove_port(port);
                kfree(port);
        }
}
EXPORT_SYMBOL(dmx512_delete_port);


void dmx512_port_set_sendframe(struct dmx512_port *port,
                               int (*callback_fn) (struct dmx512_port *,
                                                   struct dmx512_framequeue_entry *)
        )
{
        if (port)
                port->send_frame = callback_fn;
}
EXPORT_SYMBOL(dmx512_port_set_sendframe);

                                        
                                        

int dmx512_add_port(struct dmx512_device * dev, struct dmx512_port *port)
{
    int ret;
    DMX512_LOCKED(ret=_dmx512_add_port(dev, port));
    return ret;
}
EXPORT_SYMBOL(dmx512_add_port);

int dmx512_remove_port(struct dmx512_port *port)
{
    int ret;
    DMX512_LOCKED(ret=_dmx512_remove_port(port));
    return ret;
}
EXPORT_SYMBOL(dmx512_remove_port);

struct dmx512_port * dmx512_port_by_index(struct dmx512_device * device, const int index)
{
    struct dmx512_port * port;
    DMX512_LOCKED(port = _dmx512_port_by_index(device,index));
    return port;
}
EXPORT_SYMBOL(dmx512_port_by_index);

int dmx512_port_index(struct dmx512_port * port)
{
    int ret;
    DMX512_LOCKED(ret = _dmx512_port_index(port));
    return ret;
}
EXPORT_SYMBOL(dmx512_port_index);

void dmx512_port_set_userptr(struct dmx512_port * port, void * userptr)
{
  if (port)
    port->userptr = userptr;
}
EXPORT_SYMBOL(dmx512_port_set_userptr);

void * dmx512_port_userptr(struct dmx512_port * port)
{
  return port ? port->userptr : 0;
}
EXPORT_SYMBOL(dmx512_port_userptr);

const char * dmx512_port_name(struct dmx512_port * port)
{
        return port ? port->name : 0;
}
EXPORT_SYMBOL(dmx512_port_name);


int dmx512_received_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	if (!port || !port->device)
		return -1;
	frame->frame.port = dmx512_port_index(port);
	dmx512_framequeue_put(&port->device->rxframequeue, frame);
	wake_up (&port->device->rxwait_queue);

	return 0;
}
EXPORT_SYMBOL(dmx512_received_frame);

void dmx512_put_frame(struct dmx512_port *port, struct dmx512_framequeue_entry * frame)
{
	(void)port;
	/* we may later use the port to control the per port usage of frames
	   or have some accounting. */
	dmx512_framequeue_put(&free_framequeue, frame);
}
EXPORT_SYMBOL(dmx512_put_frame);

struct dmx512_framequeue_entry * dmx512_get_frame(struct dmx512_port *port)
{
	(void)port;
	/* we may later use the port to control the per port usage of frames
	   or have some accounting. */
	return dmx512_framequeue_get (&free_framequeue);
}
EXPORT_SYMBOL(dmx512_get_frame);



static int __init dmx512_core_init(void)
{
	int i;
	printk(KERN_INFO "loading dmx512 core\n");

	dmx512_framequeue_init(&free_framequeue);
	/* put some frame in the freequeue */
	for (i = 0; i < 16; ++i)
		dmx512_framequeue_put(&free_framequeue, dmx512_framequeue_entry_alloc());

        return 0;
}

static void __exit dmx512_core_exit(void)
{
    printk(KERN_INFO "unloading dmx512 core\n");
    dmx512_framequeue_cleanup(&free_framequeue);
}

module_init(dmx512_core_init);
module_exit(dmx512_core_exit);

MODULE_AUTHOR("Michael Stickel");
MODULE_DESCRIPTION("dmx512 driver core.");
MODULE_VERSION("0.01");
MODULE_LICENSE("GPL");
