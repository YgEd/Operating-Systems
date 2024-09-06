#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/current.h>

//sched current

static int hello_init(void)
{

    printk(KERN_ALERT "Hello World from Joshua Meharg(jmeharg1)\n");
    return 0;
}

static void hello_exit(void)
{
    printk(KERN_ALERT "PID = %d, program name = %s\n", current->pid, current->comm);
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("Dual BSD/GPL");