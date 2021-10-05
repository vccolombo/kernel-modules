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

static struct cdev cdev;
static struct class *cl;
static dev_t devno;

static struct file_operations buffer_fops = {
    .owner = THIS_MODULE,
};

static int buffer_setup_device(void)
{
	int err;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (unlikely(err < 0)) {
		goto out;
	}

	cdev_init(&cdev, &buffer_fops);
	cdev.owner = THIS_MODULE;
	err = cdev_add(&cdev, devno, 1);
	if (unlikely(err < 0)) {
		goto out;
	}

	return 0;

out:
	return err;
}

static int __init buffer_init(void)
{
	int err;
	struct device *dev_ret;

	err = buffer_setup_device();
	if (unlikely(err < 0)) {
		pr_err(MODULE_LOG_START "unable to setup device\n");
		goto out;
	}

	pr_info(MODULE_LOG_START "registered with major %d and minor %d\n",
		MAJOR(devno), MINOR(devno));

	cl = class_create(THIS_MODULE, MODULE_NAME);
	if (IS_ERR(cl)) {
		err = PTR_ERR(cl);
		goto unregister_chrdev;
	}

	dev_ret = device_create(cl, NULL, devno, NULL, MODULE_NAME);
	if (IS_ERR(dev_ret)) {
		err = PTR_ERR(dev_ret);
		goto class_destroy;
	}

	return 0;

class_destroy:
	class_destroy(cl);
unregister_chrdev:
	unregister_chrdev_region(devno, 1);
out:
	return err;
}

static void __exit buffer_exit(void)
{
	cdev_del(&cdev);
	device_destroy(cl, devno);
	class_destroy(cl);
	unregister_chrdev_region(devno, 1);

	pr_info(MODULE_LOG_START "unregistered");
}

module_init(buffer_init);
module_exit(buffer_exit);