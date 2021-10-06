#include <linux/cdev.h> // cdev
#include <linux/device.h>
#include <linux/fs.h> // register_chrdev_region, alloc_chrdev_region
#include <linux/init.h>
#include <linux/kdev_t.h> // MAJOR, MKDEV
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h> // dev_t

MODULE_AUTHOR("Victor Cora Colombo");
MODULE_LICENSE("GPL");

#define MODULE_NAME "buffer"
#define MODULE_LOG_START MODULE_NAME ": "

#define MAX_CHUNK_SIZE 4096

static struct class *cl;
static dev_t devno;

// I could have used the kernel list here,
// but I prefered a simpler implementation
struct buffer_data {
	char *chunk;
	struct buffer_data *next;
};

struct buffer_dev {
	struct buffer_data *data_head;
	struct buffer_data *data_tail;
	struct cdev cdev;
};

struct buffer_dev *dev;

static void clean_data(void)
{
	struct buffer_data *prev;
	while (dev->data_head) {
		prev = dev->data_head;
		dev->data_head = dev->data_head->next;

		if (prev->chunk)
			kfree(prev->chunk);
		kfree(prev);
	}
}

ssize_t buffer_write(struct file *filp, const char __user *buf, size_t count,
		     loff_t *off)
{
	long long int zero = 0;

	if (count > MAX_CHUNK_SIZE) {
		count = MAX_CHUNK_SIZE;
	}

	dev->data_tail->chunk = kmalloc(count + 1, GFP_KERNEL);
	if (!dev->data_tail->chunk) {
		return -ENOMEM;
	}

	count = simple_write_to_buffer(dev->data_tail->chunk, count, &zero, buf,
				       count);

	dev->data_tail->next = kzalloc(sizeof(struct buffer_data), GFP_KERNEL);
	dev->data_tail = dev->data_tail->next;

	*off += count;
	return count;
}

int buffer_open(struct inode *inode, struct file *filp)
{
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		clean_data();

		// initialize the list
		dev->data_head =
		    kzalloc(sizeof(struct buffer_data), GFP_KERNEL);
		dev->data_tail = dev->data_head;
	}

	return 0;
}

int buffer_release(struct inode *inode, struct file *filp) { return 0; }

static struct file_operations buffer_fops = {
    .owner = THIS_MODULE,
    .write = buffer_write,
    .open = buffer_open,
    .release = buffer_release,
};

static int buffer_setup_device(void)
{
	int err;

	dev = kzalloc(sizeof(struct buffer_dev), GFP_KERNEL);
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
		clean_data();
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