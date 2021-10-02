#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kdev_t.h> // MAJOR, MKDEV
#include <linux/fs.h>     // register_chrdev_region, alloc_chrdev_region
#include <linux/cdev.h>   // cdev
#include <linux/types.h>  // dev_t

MODULE_AUTHOR("Victor Cora Colombo");
MODULE_LICENSE("GPL");

#define MODULE_NAME "kernel_time"
#define MODULE_LOG_START MODULE_NAME ": "

static struct cdev cdev;
static struct class *cl;

dev_t devno;

static struct file_operations kernel_time_fops = {
    .owner = THIS_MODULE,
};

static int kernel_time_setup_device(void)
{
    int err;

    cdev_init(&cdev, &kernel_time_fops);
    cdev.owner = THIS_MODULE;
    err = cdev_add(&cdev, devno, 1);

    return err;
}

static int __init kernel_time_init(void)
{
    int err;
    struct device *dev_ret;

    err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
    if (unlikely(err < 0))
    {
        printk(KERN_ERR MODULE_LOG_START "unable to alloc major number\n");
        goto out;
    }

    if (IS_ERR(cl = class_create(THIS_MODULE, "chardrv")))
    {
        err = PTR_ERR(cl);
        goto unregister_chrdev;
    }

    if (IS_ERR(dev_ret = device_create(cl, NULL, devno, NULL, "chardrv")))
    {
        err = PTR_ERR(dev_ret);
        goto class_destroy;
    }

    err = kernel_time_setup_device();
    if (unlikely(err < 0))
    {
        printk(KERN_ERR MODULE_LOG_START "unable to setup device\n");
        goto device_destroy;
    }

    printk(KERN_INFO MODULE_LOG_START "registered with major %d and minor %d\n",
           MAJOR(devno), MINOR(devno));

    return 0;

device_destroy:
    device_destroy(cl, devno);
class_destroy:
    class_destroy(cl);
unregister_chrdev:
    unregister_chrdev_region(devno, 1);
out:
    return err;
}

static void __exit kernel_time_exit(void)
{
    cdev_del(&cdev);
    device_destroy(cl, devno);
    class_destroy(cl);
    unregister_chrdev_region(devno, 1);

    printk(KERN_INFO MODULE_LOG_START "unregistered");
}

module_init(kernel_time_init);
module_exit(kernel_time_exit);