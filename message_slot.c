#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include "message_slot.h"

MODULE_LICENSE("GPL");

typedef struct channel {
    unsigned int id;
    char message[MAX_MESSAGE_LENGTH];
    size_t message_length;
    struct channel* next;
} channel_t;

typedef struct slot {
    int minor;
    channel_t* channels;
    struct slot* next;
} slot_t;

typedef struct {
    unsigned int channel_id;
    int censorship_enabled;
} fd_state_t;

static int device_open(struct inode* inode, struct file* file);
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
};

static int __init message_slot_init(void) {
    int rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops);
    if (rc < 0) {
        printk(KERN_ERR "message_slot: failed to register device\n");
        return rc;
    }
    printk(KERN_INFO "message_slot: module loaded\n");
    return 0;
}

static void __exit message_slot_cleanup(void) {
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    printk(KERN_INFO "message_slot: module unloaded\n");
}

module_init(message_slot_init);
module_exit(message_slot_cleanup);