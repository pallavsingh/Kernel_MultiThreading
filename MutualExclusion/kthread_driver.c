#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>


#define DEVICE_NAME      "kthread_dev"
#define CLASS_NAME       "kthread_class"
#define NTHREADS         5
#define BUFFER_SIZE      4096


static int major_number;
static struct class *  kthread_class_ptr = NULL;
static struct device * kthread_device_ptr = NULL;

// Its Critical Section 
// Its Usage must be guarded by Mutex.

static char message[BUFFER_SIZE];
static int message_len = 0;

// Here we declare Mutex Variable used to provide Mutual Exclusion. 
// Also Store the ThreadID per thread created.

static struct task_struct *threads[NTHREADS];
static DEFINE_MUTEX(mutex_var);

// Its Mapped function to be called on read on device
static ssize_t dev_read(struct file *filep, char __user *buffer,  size_t len, loff_t *offset) {
    int bytes_to_read;

    if (*offset >= message_len)
        return 0;

    bytes_to_read = min((int)len, message_len - (int)*offset);

    if (copy_to_user(buffer, message + *offset, bytes_to_read))
        return -EFAULT;

    *offset += bytes_to_read;

    return bytes_to_read;
}


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};


// Function is shared and invoked by ALL the Threads
// -- Its take void * -- It means any data type pointer can be passed. 

static int thread_function(void *data) {
    int id = *(int *)data;
    char local_buffer[128];

// kthread_should_stop() returns nonzero when the thread has been asked to stop.
// When a stop request is pending, the condition becomes false and the loop exits.

    while (!kthread_should_stop()) {

        mutex_lock(&mutex_var);

        printk(KERN_INFO "kthread[%d]: Running...\n", id);
        snprintf(local_buffer, sizeof(local_buffer), "Kernel Thread %d is running\n", id);
        strncat(message, local_buffer,  BUFFER_SIZE - strlen(message) - 1);
        message_len = strlen(message);

        mutex_unlock(&mutex_var);

// mutex has already been released, so another thread can modify message.
        msleep(2000);
    }

    printk(KERN_INFO "kthread[%d]: Stopping...\n", id);

    return 0;
}



static int __init kthread_driver_init(void) {
    int i;
    static int ids[NTHREADS];

    printk(KERN_INFO "Initializing KThread Driver\n");

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register major number\n");
        return major_number;
    }

    kthread_class_ptr = class_create(CLASS_NAME);
    if (IS_ERR(kthread_class_ptr)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(kthread_class_ptr);
    }

    kthread_device_ptr = device_create(kthread_class_ptr,
                                   NULL,
                                   MKDEV(major_number, 0),
                                   NULL,
                                   DEVICE_NAME);

    if (IS_ERR(kthread_device_ptr)) {
        class_destroy(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(kthread_device_ptr);
    }

    memset(message, 0, BUFFER_SIZE);

// Here we are creating N Threads.
// -- We are Passing starting func to be invoked and argument to the function. 

    for (i = 0; i < NTHREADS; i++) {
        ids[i] = i;
        threads[i] = kthread_run(thread_function, &ids[i], "my_kthread_%d", i);

        if (IS_ERR(threads[i]))
            printk(KERN_ALERT "Failed to create thread %d\n", i);
    }

    printk(KERN_INFO "KThread Driver Loaded\n");
    printk(KERN_INFO "Device created: /dev/%s\n", DEVICE_NAME);

    return 0;

}


static void __exit kthread_driver_exit(void) {
    int i;

    for (i = 0; i < NTHREADS; i++)
        if (threads[i])
            kthread_stop(threads[i]);

    device_destroy(kthread_class_ptr, MKDEV(major_number, 0));

    class_unregister(kthread_class_ptr);
    class_destroy(kthread_class_ptr);

    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "KThread Driver Unloaded\n");
}


module_init(kthread_driver_init);
module_exit(kthread_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pallav Singh");
MODULE_DESCRIPTION("Modern Linux Kernel Thread Driver -- Mutual Exlusion of shared data access");
MODULE_VERSION("1.0.0");


