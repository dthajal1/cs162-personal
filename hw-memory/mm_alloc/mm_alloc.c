/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* DLL with Sentinel Nodes Implementation for mem_blocks global storage. */
static mem_block *head, *tail;

/* Initialize sentinel nodes (head and tail) if NULL.
  Head and Tail do not hold any data.
  E.g. after append(8) DLL will be
    head <-> 8 <-> tail
*/
void init_sentinel() {
  if (head == NULL) {
    head = sbrk(sizeof(mem_block));
    tail = sbrk(sizeof(mem_block));
    head->next = tail;
    head->prev = NULL;
    tail->prev = head;
    tail->next = NULL;
  }
}

void* mm_malloc(size_t size) {
  init_sentinenl(); // only runs once at the very beginning

  if (size <= 0) return NULL;

  // iterate through the block until sufficient free block found (block.size >= size)
    // if found_block.size > size
      // split block in two: one for this and another for upcoming block
  mem_block *free_block = find_free_block(size);

  if (free_block == NULL) {
    // use sbrk to malloc
    mem_block *new_block = sbrk(sizeof(mem_block) + size * sizeof(char));
    if (new_block == (void *) -1) {
      return NULL; // ?
    }
    new_block->free = false;
    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->size = size;
    append(new_block);
    return new_block->data;
  } else {
    // use existing free blocks
    if (free_block->size > size) {
      free_block = split_block(size, free_block);
    }
  }

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
    mem_block* allocated_block = (mem_block *) ptr;
    allocated_block->free = true;

    // coalesce consecutive free blocks upon freeing a block that is adjacent to other free block(s)
    coalesce_consecutive_free_blocks(ptr);
  }
}

/* Combine consecutive adjacent free blocks to form a free block. */
void coalesce_consecutive_free_blocks(mem_block* free_block) {
  // Combine to the right
  mem_block *next_ptr = free_block->next;
  while (next_ptr && next_ptr->free) {
    // merge free_block and next_ptr block
    free_block->size += next_ptr->size;
    free_block->next = next_ptr->next;
    next_ptr->next->prev = free_block; // might run into null ptr exception => need an end sentinel node (DONE)

    next_ptr = next_ptr->next;
  }

  // Combine to the left
  mem_block *prev_ptr = free_block->prev;
  while (prev_ptr && prev_ptr->free) {
    // merge free_block and prev_ptr block
    free_block->size += prev_ptr->size;
    free_block->prev = prev_ptr->prev;
    prev_ptr->prev->next = free_block; // might run into null ptr exception => need a front sentinel node (DONE)

    prev_ptr = prev_ptr->prev;
  }

  /* Ex
    Combine to right
    have: a <-> b <-> c
    curr_elem = a and both b,c are free
    want: abc
    steps:
      first: ab <-> c
      second: abc => might run into null ptr => need sentinel node
  */
}

/* Insert/Append to end of global mem_blocks. */
void append(mem_block *new_block) {
  if (head->next == NULL) {
    head->next = tail->prev = new_block;
    new_block->prev = head;
    new_block->next = tail;
  } else {
    new_block->prev = tail->prev;
    new_block->next = tail;
    tail->prev->next = tail->prev = new_block;
  }

  /* Ex. append(8)
    have: head <-> tail
    want: head <-> 8 <-> tail

    append(9)
    want: head <-> 8 <-> 9 <-> tail
  */
}

/* Find free block whose size >= SIZE. */
mem_block* find_free_block(size_t size) {
  mem_block *ptr = head->next;
  while (ptr != NULL) {
    if (ptr->free && ptr->size >= size) {
      // Question: need to zero out its data??
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

/* Split FREE_BLOCK in two blocks. First block of size SIZE and other of size FREE_BLOCK->size - SIZE.
  Return first block. Already appended? */
void* split_block(size_t size, mem_block* free_block) {
  mem_block *first_block;
  mem_block *second_block;

  first_block->prev = free_block->prev;
  free_block->prev->next = first_block; // might run into null ptr exception => need a front sentinel node (DONE)
  first_block->size = size;

  second_block->next = free_block->next;
  free_block->next->prev = second_block; // might run into null ptr exception => need an end sentinel node (DONE)
  second_block->size = free_block->size - size;

  return first_block;

  /* ex. 
    have: a <-> b <-> c
    b is large free block => split it in two
    want: a <-> d <-> e <-> c
  */
}
