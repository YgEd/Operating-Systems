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

#include "scull.h"		/* local definitions */

#include <linux/semaphore.h> // for semphore implementation

/*
 * Our parameters which can be set at load time.
 */

static int scull_major = SCULL_MAJOR;
static int scull_minor = 0;
static int scull_fifo_elemsz = SCULL_FIFO_ELEMSZ_DEFAULT; /* ELEMSZ */
static int scull_fifo_size   = SCULL_FIFO_SIZE_DEFAULT;   /* N      */

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_fifo_size, int, S_IRUGO);
module_param(scull_fifo_elemsz, int, S_IRUGO);

MODULE_AUTHOR("jmeharg1");
MODULE_LICENSE("Dual BSD/GPL");

char* buffer; //FIFO queue pointer

static DEFINE_MUTEX(msg_lock); //mutex for initialy using the buffer
struct semaphore put; //semaphore for producer
struct semaphore take; //semaphore for consumer

int q_size; //size of the

char* start; //pointers to keep track of queue positions
char* end;

int space; //holds the amount of space left in the queue;

char* head; 
/* head holds the position of the beginning of the queue to be
   able to calculate how much space is left in the queue
*/

static struct cdev scull_cdev;		/* Char device structure */

/*
 * Open and close
 */

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
 * Read and Write
 */

 // Full state: start + 1 == end
 // Empty state: start == end

static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */
	printk(KERN_INFO "scull read\n");
	if (down_interruptible(&take)){
		return(-EINTR); 
	}
	mutex_lock(&msg_lock);


	//read first four bytes of buffer to get the size of the string
	int len = (int)(*start);
	int amount; //amount to be read
	//increment past the integer
	start+=4;

	//get length of string
	amount = len;


	//printk(KERN_INFO "amount = %d\n", amount);
	//printk(KERN_INFO "start[0] = %c\n", *start);
	//copy data from kernel to user
	int n_bytes = copy_to_user(buf, start, amount);

	//printk(KERN_INFO "n_bytes copied = %d\n", n_bytes);

	//increment 
	start+=scull_fifo_elemsz;
	if (start == head+q_size){
		start = head;
	}

	//if its at the end 
	if (start == head+q_size){
		start = head;
	}

	mutex_unlock(&msg_lock);
	up(&put);
	return amount-n_bytes;
}


static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */
	printk(KERN_INFO "scull write\n");
	if (down_interruptible(&put)){
		return(-EINTR); 
	}
	mutex_lock(&msg_lock);



	int amount; //amount of chars to be written
	if (count > scull_fifo_elemsz){
		amount = scull_fifo_elemsz;
	}
	else{
		amount = count;
	}

	
	//add length field
	*end = amount;
	//increment past length
	end+=4;

	char* hold = (char*)kzalloc(amount, GFP_KERNEL); //array to hold user message in
	
	//get msg from user and store in hold
	int n_bytes = copy_from_user(hold, buf, amount);

	//add the string to the buffer
	//modulo so it warps around before you increment
	int i;
	for(i=0; i<amount; i++){
		*(end+i) = hold[i];
	}

	//increment end position
	end+=scull_fifo_elemsz;
	if (end == head+q_size){
		end = head;
	}
	

	kfree(hold);
	mutex_unlock(&msg_lock);
	up(&take);
	return amount-n_bytes;
}

/*
 * The ioctl() implementation
 */
static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
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
	case SCULL_IOCGETELEMSZ:
		return scull_fifo_elemsz;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}

struct file_operations scull_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open 		= scull_open,
	.release	= scull_release,
	.read 		= scull_read,
	.write 		= scull_write,
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

	/* TODO: free FIFO safely here */

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	//free the FIFO buffer
	kfree(buffer);


	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

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

	/* TODO: allocate FIFO correctly here */

	// this line makes kernel killed
	sema_init(&put, (int)scull_fifo_size); //initialize semaphore for producer
	sema_init(&take, 0); //

	
	q_size = scull_fifo_size*(sizeof(int)+scull_fifo_elemsz);

	printk(KERN_ALERT "q_size = %d\n", q_size);
	buffer = (char*)kzalloc(q_size, GFP_KERNEL); //initialize buffer with size provided
	if (buffer == NULL){
		printk(KERN_ALERT "kzalloc failed\n");
		return -1;
	}
	space = q_size;

	start = buffer;
	end = buffer;
	head = buffer;


	printk(KERN_INFO "scull: FIFO SIZE=%u, ELEMSZ=%u\n", scull_fifo_size, scull_fifo_elemsz);

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
