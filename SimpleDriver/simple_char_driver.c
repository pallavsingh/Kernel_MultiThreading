#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>


#define DEVICE_NAME     "mychardev"
#define BUFFER_SIZE     4096


static int major_number;
static char message[BUFFER_SIZE] = {0};
static int message_size;


// in C Language : Userdefined function signature must match -->> Linux kernel function pointer signature
static int dev_open(struct inode *inode_ptr, struct file *file_ptr) {
    pr_info("mychardev: Device opened -->> Major: %u, Minor: %u\n",  imajor(inode_ptr), iminor(inode_ptr));
    return 0;
}


// in C Language : Userdefined function signature must match -->> Linux kernel function pointer signature
static int dev_release(struct inode *inode_ptr, struct file *file_ptr) {
	pr_info("mychardev: Device closed -->> Major: %u, Minor: %u\n",  imajor(inode_ptr), iminor(inode_ptr));
	return 0;
}


// Here we are reading from devicefile and Copying it to Userspace.
// |----------|---------------|
//start      offset          message_size
//            <--------------> =  bytes_to_copy (Avialable buffer)
//            <-----> len = length kernel need to copy to userspace.

// Here target buffer is the userspace address, where we need to copy the kernel data.

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int bytes_to_copy = message_size - *offset;
    if (bytes_to_copy <= 0)
        return 0;

    if (len < bytes_to_copy)
        bytes_to_copy = len;

    if (copy_to_user(buffer, message + *offset, bytes_to_copy))
        return -EFAULT;

    *offset += bytes_to_copy;
    pr_info("mychardev: Read %d bytes\n", bytes_to_copy);
    return bytes_to_copy;
}


// Here we copy  to Target Kernel space buffer << ---- Userspace buffer
// len = Number of bytes the application requests to write.
// loff_t *offset =	Pointer to the current file position.

// Shouldnot we have moved the Buffer Offset accordingly in Kernel Buffer ????


static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    if (len > BUFFER_SIZE)
        len = BUFFER_SIZE;

    if (copy_from_user(message, buffer, len))
        return -EFAULT;

    message_size = len;
    pr_info("mychardev: Received %zu bytes\n", len);
    return len;
}


static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


static int __init initialize_driver(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("Failed to register char device \n");
        return major_number;
    }

    pr_info("mychardev: Registered -->> major number %d \n", major_number);
    pr_info("mychardev: Completed Constructor phase of driver \n");
    return 0;

}


static void __exit exit_driver(void) {
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("mychardev: Un-Registered -->> major number %d \n", major_number);
    pr_info("mychardev: Completed Destructor phase of driver \n");
    return;

}


module_init(initialize_driver);
module_exit(exit_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("PALLAV SINGH");
MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("Learning How to write simple char driver");
