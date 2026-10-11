
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>

#define DEVICE_NAME  "kthread_dev"
#define CLASS_NAME   "kthread_class"
#define NTHREADS     3
#define BUFFER_SIZE  4096

static int major_number;
static struct class *kthread_class_ptr;
static struct device *kthread_device_ptr;

static struct task_struct *threads[NTHREADS];
static int thread_ids[NTHREADS];

/* Shared critical-section data */
static char message[BUFFER_SIZE];
static int message_len;

/* Synchronization */
static DEFINE_MUTEX(mutex_var);
static DECLARE_WAIT_QUEUE_HEAD(turn_wait);

/* A = 0, B = 1, C = 2 */
static int current_turn = 0;

/* Character device read operation */
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    size_t bytes_to_read;

    if (*offset < 0)
        return -EINVAL;

    mutex_lock(&mutex_var);

    if (*offset >= message_len) {
        mutex_unlock(&mutex_var);
        return 0;
    }

    bytes_to_read = min_t(size_t, len, message_len - (size_t)*offset);

    if (copy_to_user(buffer, message + *offset, bytes_to_read)) {
        mutex_unlock(&mutex_var);
        return -EFAULT;
    }

    *offset += bytes_to_read;

    mutex_unlock(&mutex_var);

    return bytes_to_read;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
};

/*
 * Thread execution order:
 * A -> B -> C -> A -> ...
 */
static int thread_function(void *data)
{
    int id = *(int *)data;
    char thread_name = 'A' + id;
    int written;

    while (!kthread_should_stop()) {

        /*
         * Wait until it is this thread's turn,
         * or until the thread is asked to stop.
         */
        wait_event_interruptible( turn_wait,
            READ_ONCE(current_turn) == id || kthread_should_stop()
        );

        if (kthread_should_stop())
            break;

        mutex_lock(&mutex_var);

        printk(KERN_INFO "Thread %c is modifying\n", thread_name);

        if (message_len < BUFFER_SIZE - 1) {
            written = scnprintf( message + message_len, BUFFER_SIZE - message_len, "Thread %c is modifying\n", thread_name);
            message_len += written;
        }

        current_turn = (current_turn + 1) % NTHREADS;

        mutex_unlock(&mutex_var);

        wake_up_all(&turn_wait);
    }

    printk(KERN_INFO "Thread %c is stopping\n", thread_name);

    return 0;
}

static int __init kthread_driver_init(void)
{
    int i;
    int ret;

    printk(KERN_INFO "Initializing ordered KThread Driver\n");

    memset(message, 0, sizeof(message));
    message_len = 0;
    current_turn = 0;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register character device\n");
        return major_number;
    }

    kthread_class_ptr = class_create(CLASS_NAME);
    if (IS_ERR(kthread_class_ptr)) {
        ret = PTR_ERR(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    kthread_device_ptr = device_create(kthread_class_ptr, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);

    if (IS_ERR(kthread_device_ptr)) {
        ret = PTR_ERR(kthread_device_ptr);
        class_destroy(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    /* Create threads A, B, and C */
    for (i = 0; i < NTHREADS; i++) {
        thread_ids[i] = i;

        threads[i] = kthread_run( thread_function, &thread_ids[i], "thread_%c", 'A' + i);
        if (IS_ERR(threads[i])) {
            ret = PTR_ERR(threads[i]);
            threads[i] = NULL;

            printk(KERN_ERR "Failed to create thread %c\n", 'A' + i);
            goto err_threads;
        }
    }

    printk(KERN_INFO "Driver loaded: execution order A -> B -> C\n");
    printk(KERN_INFO "Device created: /dev/%s\n", DEVICE_NAME);

    return 0;

err_threads:
    /* Wake sleeping threads so they can stop. */
    wake_up_all(&turn_wait);

    while (--i >= 0) {
        if (threads[i])
            kthread_stop(threads[i]);
    }

    device_destroy(kthread_class_ptr, MKDEV(major_number, 0));
    class_destroy(kthread_class_ptr);
    unregister_chrdev(major_number, DEVICE_NAME);

    return ret;
}

/* Unload the driver */
static void __exit kthread_driver_exit(void)
{
    int i;

    /*
     * Stop every thread safely, including threads
     * waiting for their turn.
     */
    for (i = 0; i < NTHREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
            threads[i] = NULL;
            wake_up_all(&turn_wait);
        }
    }

    device_destroy(kthread_class_ptr, MKDEV(major_number, 0));
    class_destroy(kthread_class_ptr);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "Ordered KThread Driver unloaded\n");
}


module_init(kthread_driver_init);
module_exit(kthread_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pallav Singh");
MODULE_DESCRIPTION("Ordered A-B-C Kernel Threads with Mutual Exclusion");
MODULE_VERSION("1.0.0");

