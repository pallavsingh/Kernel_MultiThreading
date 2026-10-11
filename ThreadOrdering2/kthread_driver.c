
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

/* A = 1, B = 2, C = 3 */
static int current_turn = 1;
static int run_A = 1;
static int run_B = 2;
static int run_C = 3;
static bool stopping;

static ssize_t dev_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    size_t bytes_to_copy;

    if (mutex_lock_interruptible(&mutex_var))
        return -ERESTARTSYS;

    if (*offset >= message_len) {
        mutex_unlock(&mutex_var);
        return 0;
    }

    bytes_to_copy = min(count, (size_t)(message_len - *offset));

    if (copy_to_user(user_buffer, message + *offset, bytes_to_copy)) {
        mutex_unlock(&mutex_var);
        return -EFAULT;
    }

    *offset += bytes_to_copy;

    mutex_unlock(&mutex_var);

    return bytes_to_copy;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
};

static int thread_A(void *data)
{
    while (!kthread_should_stop()) {
        wait_event_interruptible( turn_wait,
            READ_ONCE(current_turn) == run_A || READ_ONCE(stopping) || kthread_should_stop()
        );

        if (kthread_should_stop() || READ_ONCE(stopping))
            break;

        if (mutex_lock_interruptible(&mutex_var))
            continue;

        if (stopping) {
            mutex_unlock(&mutex_var);
            break;
        }

        printk(KERN_INFO "Thread A is modifying\n");

        if (message_len < BUFFER_SIZE - 1) {
            message_len += scnprintf( message + message_len, BUFFER_SIZE - message_len, "Thread A is modifying\n");
        }

        current_turn = run_B;

        mutex_unlock(&mutex_var);
        wake_up_all(&turn_wait);
    }

    return 0;
}

static int thread_B(void *data)
{
    while (!kthread_should_stop()) {
        wait_event_interruptible( turn_wait, 
            READ_ONCE(current_turn) == run_B || READ_ONCE(stopping) || kthread_should_stop()
        );

        if (kthread_should_stop() || READ_ONCE(stopping))
            break;

        if (mutex_lock_interruptible(&mutex_var))
            continue;

        if (stopping) {
            mutex_unlock(&mutex_var);
            break;
        }

        printk(KERN_INFO "Thread B is modifying\n");

        if (message_len < BUFFER_SIZE - 1) {
            message_len += scnprintf( message + message_len, BUFFER_SIZE - message_len, "Thread B is modifying\n");
        }

        current_turn = run_C;

        mutex_unlock(&mutex_var);
        wake_up_all(&turn_wait);
    }

    return 0;
}

static int thread_C(void *data)
{
    while (!kthread_should_stop()) {
        wait_event_interruptible( turn_wait,
            READ_ONCE(current_turn) == run_C || READ_ONCE(stopping) || kthread_should_stop()
        );

        if (kthread_should_stop() || READ_ONCE(stopping))
            break;

        if (mutex_lock_interruptible(&mutex_var))
            continue;

        if (stopping) {
            mutex_unlock(&mutex_var);
            break;
        }

        printk(KERN_INFO "Thread C is modifying\n");

        if (message_len < BUFFER_SIZE - 1) {
            message_len += scnprintf( message + message_len, BUFFER_SIZE - message_len, "Thread C is modifying\n");
        }

        current_turn = run_A;

        mutex_unlock(&mutex_var);
        wake_up_all(&turn_wait);
    }

    return 0;
}

static int __init kthread_driver_init(void)
{
    int i;

    memset(message, 0, sizeof(message));
    message_len = 0;

    /* Start with Thread A */
    WRITE_ONCE(current_turn, run_A);
    WRITE_ONCE(stopping, false);

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register character device\n");
        return major_number;
    }

    kthread_class_ptr = class_create(CLASS_NAME);
    if (IS_ERR(kthread_class_ptr)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(kthread_class_ptr);
    }

    kthread_device_ptr = device_create( kthread_class_ptr, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);

    if (IS_ERR(kthread_device_ptr)) {
        int ret = PTR_ERR(kthread_device_ptr);

        class_destroy(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);

        return ret;
    }

    threads[0] = kthread_run(thread_A, &thread_ids[0], "thread_A");
    if (IS_ERR(threads[0])) {
        int ret = PTR_ERR(threads[0]);

        threads[0] = NULL;
        goto err_cleanup;
    }

    threads[1] = kthread_run(thread_B, &thread_ids[1], "thread_B");
    if (IS_ERR(threads[1])) {
        int ret = PTR_ERR(threads[1]);

        threads[1] = NULL;
        stopping = true;
        wake_up_all(&turn_wait);
        kthread_stop(threads[0]);
        threads[0] = NULL;

        goto err_cleanup_with_ret;
    }

    threads[2] = kthread_run(thread_C, &thread_ids[2], "thread_C");
    if (IS_ERR(threads[2])) {
        int ret = PTR_ERR(threads[2]);

        threads[2] = NULL;
        stopping = true;
        wake_up_all(&turn_wait);

        kthread_stop(threads[0]);
        kthread_stop(threads[1]);

        threads[0] = NULL;
        threads[1] = NULL;

        goto err_cleanup_with_ret;
    }

    printk(KERN_INFO "Ordered KThread Driver loaded\n");
    printk(KERN_INFO "Device created: /dev/%s\n", DEVICE_NAME);

    return 0;

err_cleanup:
    {
        int ret = PTR_ERR(threads[0]);

        device_destroy(kthread_class_ptr, MKDEV(major_number, 0));
        class_destroy(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);

        return ret;
    }

err_cleanup_with_ret:
    {
        int ret = -ENOMEM;

        device_destroy(kthread_class_ptr, MKDEV(major_number, 0));
        class_destroy(kthread_class_ptr);
        unregister_chrdev(major_number, DEVICE_NAME);

        return ret;
    }
}


static void __exit kthread_driver_exit(void)
{
    int i;

    WRITE_ONCE(stopping, true);
    wake_up_all(&turn_wait);

    for (i = 0; i < NTHREADS; i++) {
        if (threads[i]) {
            kthread_stop(threads[i]);
            threads[i] = NULL;
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
