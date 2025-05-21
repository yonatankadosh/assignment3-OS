#include <linux/module.h>       // Core header for loading LKMs into the kernel
#include <linux/kernel.h>       // Contains types, macros, functions for the kernel
#include <linux/fs.h>           // File system structures and functions
#include <linux/uaccess.h>      // Functions for copying data to/from user space
#include <linux/slab.h>         // Kernel memory allocation functions
#include <linux/init.h>         // Macros used to mark up initialization functions
#include <linux/string.h>       // String handling functions
#include <linux/ioctl.h>        // IOCTL command macros
#include <linux/types.h>        // Basic data types
#include <linux/errno.h>        // Error codes
#include <linux/mutex.h>        // Mutex locking mechanisms
#include "message_slot.h"       // Header for message slot device specifics

MODULE_LICENSE("GPL");

// Represents a communication channel within a slot, holding a message and its length
typedef struct channel {
    unsigned int id;
    char message[MAX_MESSAGE_LENGTH];
    size_t message_length;
    struct channel* next;
} channel_t;

// Represents a device slot identified by a minor number, containing linked channels
typedef struct slot {
    int minor;
    channel_t* channels;
    struct slot* next;
} slot_t;

// Represents the state of an open file descriptor, including selected channel and censorship flag
typedef struct {
    unsigned int channel_id;
    int censorship_enabled;
} fd_state_t;

// Global linked list head for all device slots currently in use
static slot_t* slot_list_head = NULL;

// Function prototypes for file operations
// Called when device file is opened
static int device_open(struct inode* inode, struct file* file);
// Called when data is read from device
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset);
// Called when data is written to device
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset);
// Called for ioctl commands on the device
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param);

// File operations structure linking to implemented functions
static int device_release(struct inode* inode, struct file* file);
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};
// device_release: Frees per-file descriptor state when device is closed
static int device_release(struct inode* inode, struct file* file) {
    if (file && file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    return 0;
}

// Module initialization function: registers the character device
static int __init message_slot_init(void) {
    int rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops);
    if (rc < 0) {
        printk(KERN_ERR "message_slot: failed to register device\n");
        return rc;
    }
    printk(KERN_INFO "message_slot: module loaded\n");
    return 0;
}

// Module cleanup function: unregisters the character device
static void __exit message_slot_cleanup(void) {
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    printk(KERN_INFO "message_slot: module unloaded\n");
}

module_init(message_slot_init);
module_exit(message_slot_cleanup);

// device_open: Handles opening the device file.
// It locates or creates a slot corresponding to the minor number,
// and allocates per-file descriptor state.
static int device_open(struct inode* inode, struct file* file) {
    int minor = iminor(inode);

    // Search for existing slot with matching minor number
    slot_t* slot = slot_list_head;
    while (slot != NULL) {
        if (slot->minor == minor) {
            break;
        }
        slot = slot->next;
    }

    // If slot not found, create a new one and add it to the global list
    if (!slot) {
        slot = kmalloc(sizeof(slot_t), GFP_KERNEL);
        if (!slot) {
            printk(KERN_ERR "message_slot: Failed to allocate slot for minor %d\n", minor);
            return -ENOMEM;
        }
        slot->minor = minor;
        slot->channels = NULL;
        slot->next = slot_list_head;
        slot_list_head = slot;
    }

    // Allocate and initialize per-file descriptor state
    fd_state_t* state = kmalloc(sizeof(fd_state_t), GFP_KERNEL);
    if (!state) {
        return -ENOMEM;
    }

    state->channel_id = 0;           // Default channel ID is 0 (no channel selected)
    state->censorship_enabled = 0;   // Censorship disabled by default
    file->private_data = state;      // Store state in file's private data

    return 0;
}

// device_ioctl: Handles ioctl commands to set channel or censorship mode
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    fd_state_t* state;

    // Validate input pointer
    if (!file || !file->private_data) {
        return -EINVAL;
    }

    state = (fd_state_t*) file->private_data;

    switch (ioctl_command_id) {
        case MSG_SLOT_CHANNEL:
            if (ioctl_param == 0) {
                return -EINVAL;
            }
            state->channel_id = (unsigned int) ioctl_param;
            return 0;

        case MSG_SLOT_SET_CEN:
            if (ioctl_param != 0 && ioctl_param != 1) {
                return -EINVAL;
            }
            state->censorship_enabled = (int) ioctl_param;
            return 0;

        default:
            return -EINVAL;
    }
}

// device_write: Write a message to the selected channel, applying censorship if enabled
// Note: No need to free old message buffer, as channel->message is fixed-size and reused.
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
    fd_state_t* state;
    slot_t* slot;
    channel_t* channel;
    int minor;
    char* kbuf;
    size_t i;

    // Validate input
    if (!file || !file->private_data || !buffer) {
        return -EINVAL;
    }
    state = (fd_state_t*) file->private_data;
    if (state->channel_id == 0) {
        return -EINVAL;
    }
    if (length == 0 || length > MAX_MESSAGE_LENGTH) {
        return -EMSGSIZE;
    }

    // Find slot for this minor
    minor = iminor(file_inode(file));
    slot = slot_list_head;
    while (slot) {
        if (slot->minor == minor)
            break;
        slot = slot->next;
    }
    if (!slot) {
        // Should not happen, but for safety
        return -EINVAL;
    }

    // Find or create channel for this channel_id
    channel = slot->channels;
    while (channel) {
        if (channel->id == state->channel_id)
            break;
        channel = channel->next;
    }
    if (!channel) {
        channel = kmalloc(sizeof(channel_t), GFP_KERNEL);
        if (!channel)
            return -ENOMEM;
        channel->id = state->channel_id;
        channel->message_length = 0;
        channel->next = slot->channels;
        slot->channels = channel;
    }

    // Allocate kernel buffer
    kbuf = kmalloc(length, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    if (copy_from_user(kbuf, buffer, length)) {
        kfree(kbuf);
        return -EFAULT;
    }

    // Apply censorship: replace every third character with '#'
    if (state->censorship_enabled) {
        size_t i;
        for (i = 2; i < length; i += 3) {
            kbuf[i] = '#';
        }
    }

    // Copy message into the channel
    memcpy(channel->message, kbuf, length);
    channel->message_length = length;
    kfree(kbuf);
    return length;
}

// device_read: Reads the last written message from the selected channel.
// Returns the number of bytes read, or an appropriate error code.
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
    fd_state_t* state;
    slot_t* slot;
    channel_t* channel;
    int minor;

    // Validate file and private data
    if (!file || !file->private_data || !buffer) {
        return -EINVAL;
    }

    state = (fd_state_t*) file->private_data;
    if (state->channel_id == 0) {
        return -EINVAL;
    }

    // Find the slot by minor number
    minor = iminor(file_inode(file));
    slot = slot_list_head;
    while (slot) {
        if (slot->minor == minor) break;
        slot = slot->next;
    }
    if (!slot) {
        return -EINVAL;
    }

    // Find the channel by ID
    channel = slot->channels;
    while (channel) {
        if (channel->id == state->channel_id) break;
        channel = channel->next;
    }
    if (!channel || channel->message_length == 0) {
        return -EWOULDBLOCK;
    }

    if (length < channel->message_length) {
        return -ENOSPC;
    }

    // Copy the message to user space
    if (copy_to_user(buffer, channel->message, channel->message_length)) {
        return -EFAULT;
    }

    return channel->message_length;
}