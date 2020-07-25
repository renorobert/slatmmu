#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kallsyms.h>

MODULE_LICENSE("GPL");

#define DEVICE_NAME     "cr3"

static struct cdev cdev;
static int dev_major;
static struct class *chardev;

unsigned long pid_to_cr3(int);
static long device_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
        .unlocked_ioctl = device_ioctl
};

static int dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
        add_uevent_var(env, "DEVMODE=%#o", 0666);
        return 0;
}

struct info {
	unsigned long pid;	// input
	unsigned long cr3;	// output
} info;

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct info info;

	if (copy_from_user((void *)&info, (void *)arg, sizeof(struct info))) {
		return -EFAULT;
	}

	info.cr3 = pid_to_cr3(info.pid);

	if (copy_to_user((void *)arg, (void *)&info, sizeof(struct info))) {
		return -EFAULT;
	}

	return 0;
}

/* https://github.com/gregvish/l1tf-poc/blob/master/devmem_allow_ko/devmem_allow.c */

typedef void (*text_poke_t)(void *addr, unsigned char *data, unsigned long len);

int patch_devmem_is_allowed(void)
{
	void *func = (void *) kallsyms_lookup_name("devmem_is_allowed");
	text_poke_t text_poke = (text_poke_t) kallsyms_lookup_name("text_poke");

	if (func == NULL || text_poke == NULL) {
		printk(KERN_INFO "No devmem_is_allowed or text_poke\n");
		return -1;
	}

	/* Replace devmem_is_allowed func with:
	   6a 01    pushq  $0x1
	   58       pop    %rax
	   c3       retq
	 */
	text_poke((void *)func, "\x6a\x01\x58\xc3", 4);

	printk(KERN_INFO "Patched out devmem_is_allowed\n");

	return 0;
}

/* https://carteryagemann.com/pid-to-cr3.html */

unsigned long pid_to_cr3(int pid)
{
	struct task_struct *task;
	struct mm_struct *mm;
	void *cr3_virt;
	unsigned long cr3_phys;

	task = pid_task(find_vpid(pid), PIDTYPE_PID);

	if (task == NULL)
		return 0; // pid has no task_struct

	mm = task->mm;

	// mm can be NULL in some rare cases (e.g. kthreads)
	// when this happens, we should check active_mm
	if (mm == NULL) {
		mm = task->active_mm;
	}

	if (mm == NULL)
		return 0; // this shouldn't happen, but just in case

	cr3_virt = (void *) mm->pgd;
	cr3_phys = virt_to_phys(cr3_virt);

	return cr3_phys;
}

static void init_device(void)
{
        int err;
        dev_t dev;

        err = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);

        dev_major = MAJOR(dev);

        chardev = class_create(THIS_MODULE, DEVICE_NAME);
        chardev->dev_uevent = dev_uevent;

        cdev_init(&cdev, &fops);
        cdev.owner = THIS_MODULE;

        cdev_add(&cdev, MKDEV(dev_major, 0), 1);

        device_create(chardev, NULL, MKDEV(dev_major, 0), NULL, DEVICE_NAME);
}

static int __init dev_init(void)
{
	printk(KERN_INFO "Loading CR3\n");
	
	init_device();
	
	if (patch_devmem_is_allowed() < 0)
		printk(KERN_INFO "Failed to patch devmem_is_allowed\n");

	return 0;
}

static void __exit dev_exit(void)
{
	device_destroy(chardev, MKDEV(dev_major, 0));
        class_unregister(chardev);
        class_destroy(chardev);
        unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);
}

module_init(dev_init);
module_exit(dev_exit);

