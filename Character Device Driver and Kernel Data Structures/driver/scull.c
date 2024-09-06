/*
 * scull.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>

#include <linux/uaccess.h>	/* copy_*_user */
#include <asm/current.h> // I think we need to add this??
#include <linux/mutex.h>

#include "scull.h"		/* local definitions */

/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

MODULE_AUTHOR("jmeharg1");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */

/*
 * Open and close
 */

int counter = 0; //counter tat holds how many tasks have used the SCULL_IOCIQUANTUM command
struct tasks* head; //head of the list

static int scull_open(struct inode *inode, struct file *filp)
{
	
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;
}

/*
 * The ioctl() implementation
 */

static DEFINE_MUTEX(list_lock);

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {

	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		break;
        
	case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;
	
	case SCULL_IOCIQUANTUM: ;
		struct task_info t;
		t.state = current->state;
		t.cpu = current->cpu;
		t.prio = current->prio;
		t.pid = current->pid;
		t.tgid = current->tgid;
		t.nvcsw = current->nvcsw;
		t.nivcsw = current->nivcsw;

		//copy task_info struct to the user
		int nbytes = copy_to_user((int __user *)arg, &t, sizeof(struct task_info));
		if (nbytes != 0){
			//copy_to_user failed
			return -1;
		}

		mutex_lock(&list_lock);
		counter++;
		//if it is the first task that initiated command
		if (counter == 1){
			head->pid = t.pid;
			head->tgid = t.tgid;
			
		}

		//if not the head
		else {
			struct tasks* curr = head;
			int found = 0;
			//see if the current task is already in the list
			while(curr->next != NULL){
				
				if (curr->pid == t.pid && curr->tgid == t.tgid){
					found = 1;
					break;
				}
				curr = curr->next;
			}
			if (curr->pid == t.pid && curr->tgid == t.tgid){
					found = 1;
				}
			
			//if it is not in the last add it
			if (found == 0){
				struct tasks* new = (struct tasks*)kmalloc(sizeof(struct tasks), GFP_KERNEL);
				new->pid = t.pid;
				new->tgid = t.tgid;
				curr->next = new;
				new->prev = curr;
				new->next = NULL;
			}
			
		}
		mutex_unlock(&list_lock);

		
		break;

		


	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	.release =  scull_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);

	//go through list and free all memebers
	struct tasks* curr = head;
	int index = 0;
	while (curr->next != NULL){
		//print each node in the list
		printk(KERN_ALERT "Task %d: PID %d, TGID %d\n", index, curr->pid, curr->tgid);
		curr = curr->next;
		index++;
	}
	//print last one
	printk(KERN_ALERT "Task %d: PID %d, TGID %d\n", index, curr->pid, curr->tgid);
	struct tasks* prev = head;
	//current now holds last element in list so free and go backwards
	while(curr != NULL){
		prev = curr->prev;
		kfree(curr);
		curr = prev;
	}
}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

	head = (struct tasks*)kmalloc(sizeof(struct tasks), GFP_KERNEL); //head of the list
	head->next = NULL;
	head->prev = NULL;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
