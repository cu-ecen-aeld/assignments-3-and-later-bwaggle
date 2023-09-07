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
#else
#include <string.h>
#include <stdio.h>
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
    for (int i = buffer->out_offs; i < arr_end; i++) {
        arr_pos = i % 10; // wraparound
        pos += buffer->entry[arr_pos].size;
        // printf("Position is %d and char_offset is %lu for buff size %lu\n", pos, char_offset, buffer->entry[arr_pos].size);
        if (pos == 0) return NULL; // empty buffer
        if (char_offset < pos) {
            // printf("Found entry %s at position %d for offset %lu\n", buffer->entry[arr_pos].buffptr, pos_start, char_offset);
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
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */

   // Write the buffer entry at the "in" offset location
   buffer->entry[buffer->in_offs] = *add_entry;
//    printf("Received entry %p\n", (void *)add_entry->buffptr);
//    printf("Entry text is %s", add_entry->buffptr);
//    printf("Added entry at location %d\n", buffer->in_offs);
//    printf("Entry at location %d is %s\n", buffer->in_offs, buffer->entry[buffer->in_offs].buffptr);
   

   // Advance the write offset location ensuring for wrap-round
   buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
//    printf("Incremented in_offs to %d\n", buffer->in_offs);

   // When the buffer is full, advance the read location "out" offset
   if (buffer->full) {
    buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    // printf("Advanced read location to %d\n", buffer->out_offs);
   }

   // Set the buffer full flag if the read and write offsets match
   if (buffer->in_offs == buffer->out_offs) {
    buffer->full = true;
    // printf("Buffer is full\n");
   } else {
    buffer->full = false;
   }
   
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
