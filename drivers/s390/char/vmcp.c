/*
 * Copyright IBM Corp. 2004,2007
 * Interface implementation for communication with the z/VM control program
 * Author(s): Christian Borntraeger <borntraeger@de.ibm.com>
 *
 *
 * z/VMs CP offers the possibility to issue commands via the diagnose code 8
 * this driver implements a character device that issues these commands and
 * returns the answer of CP.

 * The idea of this driver is based on cpint from Neale Ferguson and #CP in CMS
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <asm/cpcmd.h>
#include <asm/debug.h>
#include <asm/uaccess.h>
#include "vmcp.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Borntraeger <borntraeger@de.ibm.com>");
MODULE_DESCRIPTION("z/VM CP interface");

#define PRINTK_HEADER "vmcp: "

static debug_info_t *vmcp_debug;

static int vmcp_open(struct inode *inode, struct file *file)
{
	struct vmcp_session *session;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	session = kmalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;
	session->bufsize = PAGE_SIZE;
	session->response = NULL;
	session->resp_size = 0;
	mutex_init(&session->mutex);
	file->private_data = session;
	return nonseekable_open(inode, file);
}

static int vmcp_release(struct inode *inode, struct file *file)
{
	struct vmcp_session *session;

	session = (struct vmcp_session *)file->private_data;
	file->private_data = NULL;
	free_pages((unsigned long)session->response, get_order(session->bufsize));
	kfree(session);
	return 0;
}

static ssize_t
vmcp_read(struct file *file, char __user *buff, size_t count, loff_t *ppos)
{
	size_t tocopy;
	struct vmcp_session *session;

	session = (struct vmcp_session *)file->private_data;
	if (mutex_lock_interruptible(&session->mutex))
		return -ERESTARTSYS;
	if (!session->response) {
		mutex_unlock(&session->mutex);
		return 0;
	}
	if (*ppos > session->resp_size) {
		mutex_unlock(&session->mutex);
		return 0;
	}
	tocopy = min(session->resp_size - (size_t) (*ppos), count);
	tocopy = min(tocopy, session->bufsize - (size_t) (*ppos));

	if (copy_to_user(buff, session->response + (*ppos), tocopy)) {
		mutex_unlock(&session->mutex);
		return -EFAULT;
	}
	mutex_unlock(&session->mutex);
	*ppos += tocopy;
	return tocopy;
}

static ssize_t
vmcp_write(struct file *file, const char __user *buff, size_t count,
	   loff_t *ppos)
{
	char *cmd;
	struct vmcp_session *session;

	if (count > 240)
		return -EINVAL;
	cmd = kmalloc(count + 1, GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;
	if (copy_from_user(cmd, buff, count)) {
		kfree(cmd);
		return -EFAULT;
	}
	cmd[count] = '\0';
	session = (struct vmcp_session *)file->private_data;
	if (mutex_lock_interruptible(&session->mutex)) {
		kfree(cmd);
		return -ERESTARTSYS;
	}
	if (!session->response)
		session->response = (char *)__get_free_pages(GFP_KERNEL
						| __GFP_REPEAT | GFP_DMA,
						get_order(session->bufsize));
	if (!session->response) {
		mutex_unlock(&session->mutex);
		kfree(cmd);
		return -ENOMEM;
	}
	debug_text_event(vmcp_debug, 1, cmd);
	session->resp_size = cpcmd(cmd, session->response, session->bufsize,
				   &session->resp_code);
	mutex_unlock(&session->mutex);
	kfree(cmd);
	*ppos = 0;		/* reset the file pointer after a command */
	return count;
}


/*
 * These ioctls are available, as the semantics of the diagnose 8 call
 * does not fit very well into a Linux call. Diagnose X'08' is described in
 * CP Programming Services SC24-6084-00
 *
 * VMCP_GETCODE: gives the CP return code back to user space
 * VMCP_SETBUF: sets the response buffer for the next write call. diagnose 8
 * expects adjacent pages in real storage and to make matters worse, we
 * dont know the size of the response. Therefore we default to PAGESIZE and
 * let userspace to change the response size, if userspace expects a bigger
 * response
 */
static long vmcp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct vmcp_session *session;
	int temp;

	session = (struct vmcp_session *)file->private_data;
	if (mutex_lock_interruptible(&session->mutex))
		return -ERESTARTSYS;
	switch (cmd) {
	case VMCP_GETCODE:
		temp = session->resp_code;
		mutex_unlock(&session->mutex);
		return put_user(temp, (int __user *)arg);
	case VMCP_SETBUF:
		free_pages((unsigned long)session->response,
				get_order(session->bufsize));
		session->response=NULL;
		temp = get_user(session->bufsize, (int __user *)arg);
		if (get_order(session->bufsize) > 8) {
			session->bufsize = PAGE_SIZE;
			temp = -EINVAL;
		}
		mutex_unlock(&session->mutex);
		return temp;
	case VMCP_GETSIZE:
		temp = session->resp_size;
		mutex_unlock(&session->mutex);
		return put_user(temp, (int __user *)arg);
	default:
		mutex_unlock(&session->mutex);
		return -ENOIOCTLCMD;
	}
}

static const struct file_operations vmcp_fops = {
	.owner		= THIS_MODULE,
	.open		= vmcp_open,
	.release	= vmcp_release,
	.read		= vmcp_read,
	.write		= vmcp_write,
	.unlocked_ioctl	= vmcp_ioctl,
	.compat_ioctl	= vmcp_ioctl,
};

static struct miscdevice vmcp_dev = {
	.name	= "vmcp",
	.minor	= MISC_DYNAMIC_MINOR,
	.fops	= &vmcp_fops,
};

static int __init vmcp_init(void)
{
	int ret;

	if (!MACHINE_IS_VM) {
		PRINT_WARN("z/VM CP interface is only available under z/VM\n");
		return -ENODEV;
	}
	vmcp_debug = debug_register("vmcp", 1, 1, 240);
	if (!vmcp_debug) {
		PRINT_ERR("z/VM CP interface not loaded. Could not register "
			   "debug feature\n");
		return -ENOMEM;
	}
	ret = debug_register_view(vmcp_debug, &debug_hex_ascii_view);
	if (ret) {
		PRINT_ERR("z/VM CP interface not loaded. Could not register "
			  "debug feature view. Error code: %d\n", ret);
		debug_unregister(vmcp_debug);
		return ret;
	}
	ret = misc_register(&vmcp_dev);
	if (ret) {
		PRINT_ERR("z/VM CP interface not loaded. Could not register "
			   "misc device. Error code: %d\n", ret);
		debug_unregister(vmcp_debug);
		return ret;
	}
	PRINT_INFO("z/VM CP interface loaded\n");
	return 0;
}

static void __exit vmcp_exit(void)
{
	misc_deregister(&vmcp_dev);
	debug_unregister(vmcp_debug);
	PRINT_INFO("z/VM CP interface unloaded.\n");
}

module_init(vmcp_init);
module_exit(vmcp_exit);
