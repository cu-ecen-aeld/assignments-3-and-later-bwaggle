/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/printk.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#include "aesd-circular-buffer.h"


/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    int pos = 0;
    int pos_start = 0;
    int arr_pos = 0;
    int arr_end = buffer->out_offs + AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // Loop through the buffer entries starting at the read pointer
    // and wrapping around if necessary 
    int i;
    for (i = buffer->out_offs; i < arr_end; i++) {
        arr_pos = i % 10; // wraparound
        pos += buffer->entry[arr_pos].size;
        // PDEBUG("Position is %d and char_offset is %lu for buff size %lu", pos, char_offset, buffer->entry[arr_pos].size);
        if (pos == 0) return NULL; // empty buffer
        if (char_offset < pos) {
            PDEBUG("Found entry '%s' at position %d for offset %lu", buffer->entry[arr_pos].buffptr, pos_start, char_offset);
            *entry_offset_byte_rtn = char_offset - pos_start; // position within the entry
            return &buffer->entry[arr_pos];
        }
        pos_start += buffer->entry[arr_pos].size;
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char * aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */

   const char *ptr_return = NULL;
   const char *save_buffptr;
   uint8_t save_buffloc;

   // Save buffptr for the case of a full buffer.
   // The entry will be overwritten and the buffptr needs to 
   // be returned to the caller so it can be freed
   save_buffptr = buffer->entry[buffer->in_offs].buffptr;
   save_buffloc = buffer->in_offs;

   // Write the buffer entry at the "in/write" offset location
   buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
   buffer->entry[buffer->in_offs].size = add_entry->size;
   PDEBUG("Received entry %p", (void *)add_entry->buffptr);
   PDEBUG("Entry text is '%s'", add_entry->buffptr);
   PDEBUG("Added entry at location %d", buffer->in_offs);
   PDEBUG("Entry at location %d is '%s'", buffer->in_offs, buffer->entry[buffer->in_offs].buffptr);
   

   // Advance the write offset location ensuring for wrap-round
   buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
   PDEBUG("Incremented in_offs to %d", buffer->in_offs);

   // When the buffer is full, advance the read location "out/read" offset
   if (buffer->full) {
    // Set return value to saved buffer pointer that will be overwritten
    // so caller can free the memory
    ptr_return = save_buffptr;

    PDEBUG("Buffer is full, returning %p for circ buffer pos %d", ptr_return, save_buffloc);
    PDEBUG("Advanced read location to %d", buffer->out_offs);

    // Advance circular buffer "out/read" ptr location
    buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
   }

   // Set the buffer full flag if the read and write ptr locations match
   if (buffer->in_offs == buffer->out_offs) {
    buffer->full = true;
   } else {
    buffer->full = false;
   }

   // Return overwritten ptr location so caller can free memory
   return ptr_return;
   
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
