/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static struct mem_block *sentinel;

void* mm_malloc(size_t size) {
  //TODO: Implement malloc
  if (size <= 0) return NULL;

  // iterate through the block until sufficient free block found (block.size >= size)
    // if found_block.size > size
      // split block in two: one for this and another for upcoming block
  struct mem_block *free_block = find_free_block(size);

  if (free_block == NULL) {
    // use sbrk to malloc
  } else {
    // use existing free blocks
    if (free_block->size > size) {
      free_block = split_block(size, free_block);
    }
  }

  struct mem_block *head = &sentinel;

  struct mem_block new_block = {.free = false, .size = size, .next = NULL, .prev = NULL};
  
  // use sbrk to allocate space for mem_block + metadata

  return NULL;
}

void* mm_realloc(void* ptr, size_t size) {
  // Edge cases
  if (ptr == NULL && size == 0) {
    return NULL;
  } else if (ptr == NULL && size > 0) {
    return mm_malloc(size);
  } else if (ptr != NULL && size == 0) {
    mm_free(ptr);
    return NULL;
  }

  // mm_free(ptr)
  // mm_malloc(size)
  // zero-fill the block, and finally memcpy the old data to the new block

  return NULL;
}

void mm_free(void* ptr) {
  if (ptr != NULL) {
    // mark the mem_block pointed to by ptr as free
    struct mem_block* allocated_block = (struct mem_block *) ptr;
    allocated_block->free = true;

    // coalesce consecutive free blocks upon freeing a block that is adjacent to other free block(s)
    coalesce_consecutive_free_blocks(ptr);
  }
}

/* Combine consecutive adjacent free blocks to form a free block. */
void coalesce_consecutive_free_blocks(struct mem_block* ptr) {

}

/* Find free block whose size >= SIZE. */
void* find_free_block(size_t size) {

}

/* Split FREE_BLOCK in two blocks. First block of size SIZE and other of size FREE_BLOCK->size - SIZE.
  Return first block. Already appended? */
void* split_block(size_t size, struct mem_block* free_block) {

}
