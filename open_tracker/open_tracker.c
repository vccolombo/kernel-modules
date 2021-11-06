// includes
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

#define MODULE_NAME "open_tracker"
#define MODULE_LOG_START MODULE_NAME ": "

static int has_locks = 1;
module_param(has_locks, int, 0600);
MODULE_PARM_DESC(has_locks,
		 "If 0, this module is an example of race condition. "
		 "Otherwise, spinlocks garantee its safety");

static struct class *cl;
static dev_t devno;

struct open_tracker_dev {
	struct cdev cdev;
	u16 readers;
	u16 writers;
	spinlock_t lock;
};

static struct open_tracker_dev *dev;

static int open_tracker_open(struct inode *inode, struct file *file)
{
	int w, r;

	if (has_locks)
		spin_lock(&dev->lock);

	// this code is purposefully weird to increase the chances of a race
	// condition to happen when has_lock is 0. This module serves as an
	// example of race conditions and spinlocks
	w = dev->writers;
	r = dev->readers;

	if (file->f_mode && FMODE_WRITE) {
		w++;
	}
	if (file->f_mode && FMODE_READ) {
		r++;
	}

	pr_info(MODULE_LOG_START "readers=%d writers=%d\n", w, r);

	dev->writers = w;
	dev->readers = r;

	if (has_locks)
		spin_unlock(&dev->lock);

	return 0;
}

static int open_tracker_release(struct inode *inode, struct file *file)
{
	if (has_locks)
		spin_lock(&dev->lock);

	if (file->f_mode && FMODE_WRITE) {
		dev->writers--;
	}
	if (file->f_mode && FMODE_READ) {
		dev->readers--;
	}

	// pr_info(MODULE_LOG_START "readers=%d writers=%d\n", dev->readers,
	// 	dev->writers);

	if (has_locks)
		spin_unlock(&dev->lock);

	return 0;
}

static struct file_operations open_tracker_fops = {
    .owner = THIS_MODULE,
    .open = open_tracker_open,
    .release = open_tracker_release,
};

static int open_tracker_setup_device(void)
{
	int err;

	dev = kzalloc(sizeof(struct open_tracker_dev), GFP_KERNEL);
	if (unlikely(dev == NULL)) {
		return -ENOMEM;
	}

	spin_lock_init(&dev->lock);

	cdev_init(&dev->cdev, &open_tracker_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);

	return err;
}

static void open_tracker_cleanup(void)
{
	if (dev) {
		cdev_del(&dev->cdev);
		kfree(dev);
	}

	unregister_chrdev_region(devno, 1);
}

static int __init open_tracker_init(void)
{
	int err;
	struct device *dev_ret;

	err = alloc_chrdev_region(&devno, 0, 1, MODULE_NAME);
	if (unlikely(err)) {
		pr_err(MODULE_LOG_START "unable to alloc major number\n");
		goto fail;
	}

	err = open_tracker_setup_device();
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
	open_tracker_cleanup();
fail:
	return err;
}

static void __exit open_tracker_exit(void)
{
	device_destroy(cl, devno);
	class_destroy(cl);
	open_tracker_cleanup();

	pr_info(MODULE_LOG_START "unregistered");
}

module_init(open_tracker_init);
module_exit(open_tracker_exit);