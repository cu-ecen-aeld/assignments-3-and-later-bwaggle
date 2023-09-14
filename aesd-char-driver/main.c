/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("bwaggle"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("-->OPEN MODULE");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    // PDEBUG("<<- OPEN MODULE: filep->private_data = %p, inode->cdev = %p", filp->private_data, inode->i_cdev);
    PDEBUG("<-- OPEN MODULE");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("-->RELEASE MODULE");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t bytes_not_copied;
    struct aesd_dev *ptr_aesd_dev = (struct aesd_dev *)filp->private_data;
    struct aesd_buffer_entry *ptr_circ_buff_entry;
    size_t entry_offset;

    PDEBUG("-->AESD_READ");
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    // Lock the device
    if (mutex_lock_interruptible(&aesd_device.lock)) {
        return -EINTR;
    }

    // Read from the circular buffer
    ptr_circ_buff_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(ptr_aesd_dev->circular_buffer),
                                                                         *f_pos,
                                                                         &entry_offset);
    // No entry indicates the buffer is empty
    if (!ptr_circ_buff_entry) {
        mutex_unlock(&aesd_device.lock);
        PDEBUG("No more entries to read");
        return 0;
    }
    
    // Copy the contents received from the circular buffer back to the user space buffer
    bytes_not_copied = copy_to_user(buf, ptr_circ_buff_entry->buffptr, ptr_circ_buff_entry->size);

    // Size of buffer entry must be returned to the caller
    retval = ptr_circ_buff_entry->size;

    // Give the caller the offset position
    *f_pos += ptr_circ_buff_entry->size;

    PDEBUG("Driver read %lu bytes with offset %lld from circ buffer",retval,*f_pos);
    PDEBUG("Driver read string '%s'", ptr_circ_buff_entry->buffptr);
    PDEBUG("<--AESD_READ");

    // Unlock the device
    mutex_unlock(&aesd_device.lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    void *k_buf; // kernel space buffer
    void *temp_buf; // temp buffer for partial writes without '/n'
    const char *entry_val; // track circ buff ptr that need freed 
    struct aesd_dev *ptr_aesd_dev = (struct aesd_dev *)filp->private_data;
    size_t usr_count;

    PDEBUG("-->AESD_WRITE");
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    // Lock the aesd device
    if (mutex_lock_interruptible(&aesd_device.lock)) {
        return -EINTR;
    }

    // Allocate kernel space buffer (+1 to for print statements that need a \0 char)
    if (!(k_buf = kmalloc(count, GFP_KERNEL))) {
        printk(KERN_ERR "Failed to allocate memory of size %lu", count);
        mutex_unlock(&aesd_device.lock);
        return -ENOMEM;
    }

    // Copy the user space buffer to kernel space
    usr_count = count - copy_from_user((char *)k_buf, buf, count);
    PDEBUG("Copy from user '%s' to '%s' %lu =? %ld", buf, (char *)k_buf, count, usr_count);

    // 1. Check for a newline
    // 2. if (!newline):
    // 3.   write the partial entry to aesd buffer entry
    // 4.       create a temp buff to hold current k_buf + number of bytes in new request
    // 5.       copy from buff_entry to temp_buff current contents
    // 6.       append k_buf to temp_buff
    // 7.       copy temp_buff back to buff_entry
    // 8. else
    // 9.   write the full entry to the circular buffer

    if ( *((char *)k_buf + count - 1) != '\n') {
        PDEBUG("Starting a partial write");
        if (ptr_aesd_dev->buffer_entry.size == 0) {
            // Inital partial write to the buffer entry
            ptr_aesd_dev->buffer_entry.buffptr = (char *)k_buf;
            ptr_aesd_dev->buffer_entry.size = usr_count;

            // Set the user space position
            *f_pos = usr_count;
            retval = usr_count;
        } else {
            // Allocate temporary buffer of a size that will hold previous and new write
            if (!(temp_buf = kmalloc(ptr_aesd_dev->buffer_entry.size + usr_count, GFP_KERNEL))) {
                printk(KERN_ERR "Failed to allocate memory of size %lu", count);
                mutex_unlock(&aesd_device.lock);
                return -ENOMEM;
            }
            // Copy previous write to temp buffer
            memcpy(temp_buf, ptr_aesd_dev->buffer_entry.buffptr, ptr_aesd_dev->buffer_entry.size);

            // Append new write to temp buffer
            memcpy(temp_buf + ptr_aesd_dev->buffer_entry.size, k_buf, usr_count);

            // Free previous partial buffer entry and k_buf
            if (ptr_aesd_dev->buffer_entry.buffptr) {
                kfree(ptr_aesd_dev->buffer_entry.buffptr);
                kfree(k_buf);
            }

            // Populate the circular buffer entry with new buffer entry
            ptr_aesd_dev->buffer_entry.buffptr = (char *)temp_buf;
            ptr_aesd_dev->buffer_entry.size += usr_count;

            // Update return values
            *f_pos = ptr_aesd_dev->buffer_entry.size;
            retval = ptr_aesd_dev->buffer_entry.size;
        }
    } else {
        if (ptr_aesd_dev->buffer_entry.size == 0) {
            // Populate the circular buffer entry with pointer to kernel space buffer and size
            ptr_aesd_dev->buffer_entry.buffptr = (char *)k_buf;
            ptr_aesd_dev->buffer_entry.size += usr_count;

            // Set the user space position
            *f_pos = usr_count;
            retval = usr_count;

            PDEBUG("Driver writing string '%s' with %ld bytes", (char *)k_buf, usr_count);

            // Add the entry to the circular buffer
            if((entry_val = 
                aesd_circular_buffer_add_entry(&(ptr_aesd_dev->circular_buffer), &(ptr_aesd_dev->buffer_entry)))) {
                    PDEBUG("Freeing memory");
                    kfree(entry_val);
            }

            // Clear the entry for the next call
            ptr_aesd_dev->buffer_entry.buffptr = NULL;
            ptr_aesd_dev->buffer_entry.size = 0;
        } else {
            // Allocate temporary buffer of a size that will hold previous and new write
            if (!(temp_buf = kmalloc(ptr_aesd_dev->buffer_entry.size + usr_count, GFP_KERNEL))) {
                printk(KERN_ERR "Failed to allocate memory of size %lu", count);
                mutex_unlock(&aesd_device.lock);
                return -ENOMEM;
            }
            // Copy previous write to temp buffer
            memcpy(temp_buf, ptr_aesd_dev->buffer_entry.buffptr, ptr_aesd_dev->buffer_entry.size);

            // Append new write to temp buffer
            memcpy(temp_buf + ptr_aesd_dev->buffer_entry.size, k_buf, usr_count);

            // Free previous partial buffer entry and k_buf
            if (ptr_aesd_dev->buffer_entry.buffptr) {
                kfree(ptr_aesd_dev->buffer_entry.buffptr);
                kfree(k_buf);
            }

            // Populate the circular buffer entry with new buffer entry
            ptr_aesd_dev->buffer_entry.buffptr = (char *)temp_buf;
            ptr_aesd_dev->buffer_entry.size += usr_count;

            // Update return values
            *f_pos = ptr_aesd_dev->buffer_entry.size;
            retval = ptr_aesd_dev->buffer_entry.size;

            // Add the entry to the circular buffer
            if((entry_val = 
                aesd_circular_buffer_add_entry(&(ptr_aesd_dev->circular_buffer), &(ptr_aesd_dev->buffer_entry)))) {
                    PDEBUG("Freeing memory");
                    kfree(entry_val);
            }

            // Clear the entry for the next call
            ptr_aesd_dev->buffer_entry.buffptr = NULL;
            ptr_aesd_dev->buffer_entry.size = 0;
        }

    }


    // Unlock the aesd device
    mutex_unlock(&aesd_device.lock);

    PDEBUG("<--AESD_WRITE %lu", retval);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);
    PDEBUG("-->SETUP CDEV");

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    PDEBUG("<--SETUP_CDEV");
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    PDEBUG("-->INIT MODULE");
    
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));


    /**
     * TODO: initialize the AESD specific portion of the device
     */

    // Initialize circular buffer and its entry
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    aesd_device.buffer_entry.buffptr = NULL;
    aesd_device.buffer_entry.size = 0;
    mutex_init(&aesd_device.lock);


    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }

    PDEBUG("<--INIT_MODULE, result=%d, &aesd_device = %p", result, &aesd_device);
    return result;

}

void aesd_cleanup_module(void)
{

    dev_t devno = MKDEV(aesd_major, aesd_minor);
    int index;
    struct aesd_buffer_entry *entry;
    PDEBUG("-->CLEANUP MODULE");

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    if (mutex_lock_interruptible(&aesd_device.lock)) {
        return;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index) {
        if(entry->buffptr) {
            kfree(aesd_device.buffer_entry.buffptr);
        }
    }

    if (aesd_device.buffer_entry.buffptr) {
        kfree(aesd_device.buffer_entry.buffptr);
    }
    mutex_unlock(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
    PDEBUG("<--CLEANUP MODULE");
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
