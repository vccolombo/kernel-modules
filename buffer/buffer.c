#include <linux/cdev.h> // cdev
#include <linux/device.h>
#include <linux/fs.h> // register_chrdev_region, alloc_chrdev_region
#include <linux/init.h>
#include <linux/kdev_t.h> // MAJOR, MKDEV
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/types.h> // dev_t

MODULE_AUTHOR("Victor Cora Colombo");
MODULE_LICENSE("GPL");

#define MODULE_NAME "buffer"
#define MODULE_LOG_START MODULE_NAME ": "

static struct class *cl;
static dev_t devno;

struct buffer_dev {
	struct cdev cdev;
};

struct buffer_dev *dev;

ssize_t buffer_read(struct file *filp, char __user *buf, size_t count,
		    loff_t *off)
{
	return 0;
}

int buffer_release(struct inode *inode, struct file *filp) { return 0; }

static struct file_operations buffer_fops = {
    .owner = THIS_MODULE,
    .read = buffer_read,
    .release = buffer_release,
};

static int buffer_setup_device(void)
{
	int err;

	dev =
	    (struct buffer_dev *)kzalloc(sizeof(struct buffer_dev), GFP_KERNEL);
	if (unlikely(!dev)) {
		err = -ENOMEM;
		goto out;
	}

	cdev_init(&dev->cdev, &buffer_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);

out:
	return err;
}

static void buffer_cleanup(void)
{
	if (dev) {
		cdev_del(&dev->cdev);
		kfree(dev);
	}

	unregister_chrdev_region(devno, 1);
}

static int __init buffer_init(void)
{
	int err;
	struct device *dev_ret;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (unlikely(err < 0)) {
		pr_err(MODULE_LOG_START "unable to alloc major number\n");
		goto fail;
	}

	err = buffer_setup_device();
	if (unlikely(err < 0)) {
		pr_err(MODULE_LOG_START "unable to setup device\n");
		goto dev_cleanup;
	}

	cl = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(cl)) {
		err = PTR_ERR(cl);
		goto dev_cleanup;
	}

	dev_ret = device_create(cl, NULL, devno, NULL, MODULE_NAME);
	if (IS_ERR(dev_ret)) {
		err = PTR_ERR(dev_ret);
		goto class_destroy;
	}

	pr_info(MODULE_LOG_START "registered with major %d and minor %d\n",
		MAJOR(devno), MINOR(devno));

	return 0;

class_destroy:
	class_destroy(cl);
dev_cleanup:
	buffer_cleanup();
fail:
	return err;
}

static void __exit buffer_exit(void)
{
	device_destroy(cl, devno);
	class_destroy(cl);
	buffer_cleanup();

	pr_info(MODULE_LOG_START "unregistered");
}

module_init(buffer_init);
module_exit(buffer_exit);