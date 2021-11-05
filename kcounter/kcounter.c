#include <linux/cdev.h> // cdev
#include <linux/device.h>
#include <linux/fs.h> // register_chrdev_region, alloc_chrdev_region
#include <linux/init.h>
#include <linux/kdev_t.h> // MAJOR, MKDEV
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h> // dev_t

MODULE_AUTHOR("Victor Cora Colombo");
MODULE_LICENSE("GPL");

#define MODULE_NAME "kcounter"
#define MODULE_LOG_START MODULE_NAME ": "

static struct class *cl;
static dev_t devno;

struct counter_dev {
	struct cdev cdev;
	long counter;
	struct mutex counter_mtx;
};

static struct counter_dev *dev;

ssize_t counter_write(struct file *filp, const char __user *buf, size_t count,
		      loff_t *off)
{
	mutex_lock(&dev->counter_mtx);
	dev->counter += count;
	pr_debug(MODULE_LOG_START "counter is now %ld\n", dev->counter);
	mutex_unlock(&dev->counter_mtx);

	*off += count;
	return count;
}

ssize_t counter_read(struct file *filp, char __user *buf, size_t count,
		     loff_t *off)
{
	size_t read = 0;
	long counter;

	if (*off == sizeof(dev->counter))
		return 0;

	mutex_lock(&dev->counter_mtx);
	counter = dev->counter;
	mutex_unlock(&dev->counter_mtx);

	read += simple_read_from_buffer(buf, count, off, (const void *)&counter,
					sizeof(dev->counter));

	pr_debug(MODULE_LOG_START "read %ld bytes\n", read);

	return read;
}

static struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .write = counter_write,
    .read = counter_read,
};

static int counter_setup_device(void)
{
	int err;

	dev = kzalloc(sizeof(struct counter_dev), GFP_KERNEL);
	if (unlikely(dev == NULL)) {
		return -ENOMEM;
	}

	mutex_init(&dev->counter_mtx);

	cdev_init(&dev->cdev, &counter_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);

	return err;
}

static void counter_cleanup(void)
{
	if (dev) {
		cdev_del(&dev->cdev);
		mutex_destroy(&dev->counter_mtx);
		kfree(dev);
	}

	unregister_chrdev_region(devno, 1);
}

static int __init counter_init(void)
{
	int err;
	struct device *dev_ret;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (unlikely(err < 0)) {
		pr_err(MODULE_LOG_START "unable to alloc major number\n");
		goto fail;
	}

	err = counter_setup_device();
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
	counter_cleanup();
fail:
	return err;
}

static void __exit counter_exit(void)
{
	device_destroy(cl, devno);
	class_destroy(cl);
	counter_cleanup();

	pr_info(MODULE_LOG_START "unregistered");
}

module_init(counter_init);
module_exit(counter_exit);