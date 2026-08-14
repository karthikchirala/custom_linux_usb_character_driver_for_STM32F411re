#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/slab.h>

/*─────────────────────────────────────
  Device IDs - for stm32 device
  ─────────────────────────────────────*/

#define VENDOR_ID       0x0002
#define PRODUCT_ID      0x5750

#define MAX_PKT_SIZE    64

/*─────────────────────────────────────
  Per-Device Structure
  
  Holds all data for each connected
  STM32 device. Supports multiple
  devices if needed.
  ─────────────────────────────────────*/

struct stm32_dev
{
    struct usb_device    *usbdev;
    struct usb_interface *interface;
    unsigned char         bulk_in;
    unsigned char         bulk_out;
    struct mutex          io_mutex;
    struct kref           kref;
};
static struct usb_driver stm32_driver;
static struct usb_class_driver stm32_class;

/*─────────────────────────────────────
  Reference Count Delete
  
  Called when last reference is released
  ─────────────────────────────────────*/

static void stm32_delete(struct kref *kref)
{
    struct stm32_dev *dev;

    dev = container_of(kref,
                       struct stm32_dev,
                       kref);

    usb_put_dev(dev->usbdev);
    kfree(dev);

    printk(KERN_INFO "STM32: Device memory freed\n");
}

/*─────────────────────────────────────
  Open Function
  
  Called when user application opens
  /dev/stm32_usb
  ─────────────────────────────────────*/

static int stm32_open(struct inode *inode,
                      struct file *file)
{
    struct stm32_dev     *dev;
    struct usb_interface *interface;
    int subminor;

    subminor = iminor(inode);

    interface = usb_find_interface(&stm32_driver,
                                   subminor);

    if(!interface)
    {
        printk(KERN_ERR "STM32: Cannot find interface\n");
        return -ENODEV;
    }

    dev = usb_get_intfdata(interface);

    if(!dev)
    {
        printk(KERN_ERR "STM32: Cannot get device data\n");
        return -ENODEV;
    }

    kref_get(&dev->kref);

    file->private_data = dev;

    printk(KERN_INFO "STM32: Device Opened\n");

    return 0;
}

/*─────────────────────────────────────
  Release Function
  
  Called when user application closes
  /dev/stm32_usb
  ─────────────────────────────────────*/

static int stm32_release(struct inode *inode,
                         struct file *file)
{
    struct stm32_dev *dev;

    dev = file->private_data;

    if(!dev)
        return -ENODEV;

    kref_put(&dev->kref, stm32_delete);

    printk(KERN_INFO "STM32: Device Closed\n");

    return 0;
}

/*─────────────────────────────────────
  Write Function
  
  Called when user application writes
  to /dev/stm32_usb
  
  Sends command to STM32
  ─────────────────────────────────────*/

static ssize_t stm32_write(struct file *file,
                           const char __user *buffer,
                           size_t count,
                           loff_t *ppos)
{
    struct stm32_dev *dev;
    unsigned char    *data;
    int               retval;
    int               actual_length;

    dev = file->private_data;

    if(!dev)
    {
        printk(KERN_ERR "STM32: No device in write\n");
        return -ENODEV;
    }

    if(count > MAX_PKT_SIZE)
        count = MAX_PKT_SIZE;

    data = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);

    if(!data)
    {
        printk(KERN_ERR "STM32: kmalloc failed\n");
        return -ENOMEM;
    }

    if(copy_from_user(data, buffer, count))
    {
        kfree(data);
        printk(KERN_ERR "STM32: copy_from_user failed\n");
        return -EFAULT;
    }

    mutex_lock(&dev->io_mutex);

    if(!dev->interface)
    {
        mutex_unlock(&dev->io_mutex);
        kfree(data);
        printk(KERN_ERR "STM32: Device disconnected\n");
        return -ENODEV;
    }

    retval = usb_bulk_msg(dev->usbdev,
             usb_sndbulkpipe(dev->usbdev,
                             dev->bulk_out),
             data,
             count,
             &actual_length,
             1000);

    mutex_unlock(&dev->io_mutex);

    kfree(data);

    if(retval)
    {
        printk(KERN_ERR "STM32: Write Failed: %d\n",
               retval);
        return retval;
    }

    printk(KERN_INFO "STM32: Wrote %d bytes\n",
           actual_length);

    return actual_length;
}

/*─────────────────────────────────────
  Read Function
  
  Called when user application reads
  from /dev/stm32_usb
  
  Receives ACK (0xAA) from STM32
  ─────────────────────────────────────*/

static ssize_t stm32_read(struct file *file,
                          char __user *buffer,
                          size_t count,
                          loff_t *ppos)
{
    struct stm32_dev *dev;
    unsigned char    *data;
    int               retval;
    int               actual_length;

    dev = file->private_data;

    if(!dev)
    {
        printk(KERN_ERR "STM32: No device in read\n");
        return -ENODEV;
    }

    data = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);

    if(!data)
    {
        printk(KERN_ERR "STM32: kmalloc failed\n");
        return -ENOMEM;
    }

    mutex_lock(&dev->io_mutex);

    if(!dev->interface)
    {
        mutex_unlock(&dev->io_mutex);
        kfree(data);
        printk(KERN_ERR "STM32: Device disconnected\n");
        return -ENODEV;
    }

    retval = usb_bulk_msg(dev->usbdev,
             usb_rcvbulkpipe(dev->usbdev,
                             dev->bulk_in),
             data,
             MAX_PKT_SIZE,
             &actual_length,
             0);

    mutex_unlock(&dev->io_mutex);

    if(retval)
    {
        printk(KERN_ERR "STM32: Read Failed: %d\n",
               retval);
        kfree(data);
        return retval;
    }

    if(copy_to_user(buffer, data, actual_length))
    {
        kfree(data);
        printk(KERN_ERR "STM32: copy_to_user failed\n");
        return -EFAULT;
    }

    kfree(data);

    printk(KERN_INFO "STM32: Read %d bytes\n",
           actual_length);

    return actual_length;
}

/*─────────────────────────────────────
  File Operations Structure
  ─────────────────────────────────────*/

static struct file_operations stm32_fops =
{
    .owner   = THIS_MODULE,
    .open    = stm32_open,
    .release = stm32_release,
    .read    = stm32_read,
    .write   = stm32_write,
};

/*─────────────────────────────────────
  Probe Function
  
  Called when USB device matching
  VID/PID is connected
  ─────────────────────────────────────*/

static int stm32_probe(struct usb_interface *interface,
                       const struct usb_device_id *id)
{
    struct stm32_dev             *dev;
    struct usb_host_interface    *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int    i;
    int    retval;

    printk(KERN_INFO "STM32: Probe called\n");

    dev = kmalloc(sizeof(struct stm32_dev),
                  GFP_KERNEL);

    if(!dev)
    {
        printk(KERN_ERR "STM32: Out of memory\n");
        return -ENOMEM;
    }

    memset(dev, 0, sizeof(*dev));

    kref_init(&dev->kref);
    mutex_init(&dev->io_mutex);

    dev->usbdev    = usb_get_dev(
                     interface_to_usbdev(interface));
    dev->interface = interface;

    iface_desc = interface->cur_altsetting;

    /* Find Bulk IN and OUT endpoints */
    for(i = 0;
        i < iface_desc->desc.bNumEndpoints;
        i++)
    {
        endpoint =
        &iface_desc->endpoint[i].desc;

        if(usb_endpoint_is_bulk_in(endpoint))
        {
            dev->bulk_in =
            endpoint->bEndpointAddress;

            printk(KERN_INFO
                   "STM32: Bulk IN  EP: 0x%02x\n",
                   dev->bulk_in);
        }

        if(usb_endpoint_is_bulk_out(endpoint))
        {
            dev->bulk_out =
            endpoint->bEndpointAddress;

            printk(KERN_INFO
                   "STM32: Bulk OUT EP: 0x%02x\n",
                   dev->bulk_out);
        }
    }

    if(!dev->bulk_in || !dev->bulk_out)
    {
        printk(KERN_ERR
               "STM32: Could not find both bulk endpoints\n");
        kfree(dev);
        return -ENODEV;
    }

    usb_set_intfdata(interface, dev);

    stm32_class.name       = "stm32_usb%d";
    stm32_class.fops       = &stm32_fops;
    stm32_class.minor_base = 0;

    retval = usb_register_dev(interface,
                              &stm32_class);

    if(retval)
    {
        printk(KERN_ERR
               "STM32: Device Registration Failed: %d\n",
               retval);
        usb_set_intfdata(interface, NULL);
        kfree(dev);
        return retval;
    }

    printk(KERN_INFO
           "STM32 Connected Successfully\n");
    printk(KERN_INFO
           "STM32: Device node created at /dev/stm32_usb%d\n",
           interface->minor);

    return 0;
}

/*─────────────────────────────────────
  Disconnect Function
  
  Called when USB device is disconnected
  ─────────────────────────────────────*/

static void stm32_disconnect(
             struct usb_interface *interface)
{
    struct stm32_dev *dev;

    dev = usb_get_intfdata(interface);

    usb_set_intfdata(interface, NULL);

    usb_deregister_dev(interface, &stm32_class);

    mutex_lock(&dev->io_mutex);
    dev->interface = NULL;
    mutex_unlock(&dev->io_mutex);

    kref_put(&dev->kref, stm32_delete);

    printk(KERN_INFO "STM32 Disconnected\n");
}

/*─────────────────────────────────────
  Device Table
  
  Defines which VID/PID this driver
  should handle
  ─────────────────────────────────────*/

static struct usb_device_id stm32_table[] =
{
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {}
};

MODULE_DEVICE_TABLE(usb, stm32_table);

/*─────────────────────────────────────
  USB Driver Structure
  ─────────────────────────────────────*/

static struct usb_driver stm32_driver =
{
    .name       = "stm32_usb_driver",
    .probe      = stm32_probe,
    .disconnect = stm32_disconnect,
    .id_table   = stm32_table,
};

/*─────────────────────────────────────
  Module Init
  ─────────────────────────────────────*/

static int __init stm32_init(void)
{
    int retval;

    retval = usb_register(&stm32_driver);

    if(retval)
        printk(KERN_ERR
               "STM32: Driver Registration Failed: %d\n",
               retval);
    else
        printk(KERN_INFO
               "STM32 USB Driver Loaded Successfully\n");

    return retval;
}

/*─────────────────────────────────────
  Module Exit
  ─────────────────────────────────────*/

static void __exit stm32_exit(void)
{
    usb_deregister(&stm32_driver);

    printk(KERN_INFO
           "STM32 USB Driver Unloaded\n");
}

module_init(stm32_init);
module_exit(stm32_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CDAC ESD-040506");
MODULE_DESCRIPTION("STM32F411RE USB Character Driver");
