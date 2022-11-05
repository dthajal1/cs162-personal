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

void coalesce_consecutive_free_blocks(mem_block* free_block);
void append(mem_block *new_block);
mem_block* find_free_block(size_t size);
void* split_block(size_t size, mem_block* free_block);
mem_block* find_block(void *data);

/* Initialize sentinel nodes (head and tail) if NULL.
  Head and Tail do not hold any data.
  E.g. after append(8) DLL will be
    head <-> 8 <-> tail
*/
void init_sentinel() {
  head = sbrk(sizeof(mem_block));
  tail = sbrk(sizeof(mem_block));
  head->next = tail;
  head->prev = NULL;
  tail->prev = head;
  tail->next = NULL;
  head->free = tail->free = false;
  head->size = tail->size = 0;
}

void* mm_malloc(size_t size) {
  if (head == NULL) {
    init_sentinel(); // only runs once at the very beginning
  }
  if (size <= 0) return NULL;
  mem_block *free_block = find_free_block(size);
  if (free_block != NULL) {
    if (free_block->size > size) {
      free_block = split_block(size, free_block);
    }
    return free_block->data;
  } else { // use sbrk to malloc
    mem_block *new_block = sbrk(sizeof(mem_block) + size * sizeof(char));
    if (new_block != (void *) -1) {
      new_block->free = false;
      new_block->next = NULL;
      new_block->prev = NULL;
      new_block->size = size;
      append(new_block);
      memset(new_block->data, 0, new_block->size);
      return new_block->data;
    }
    return NULL;
  }
}

void* mm_realloc(void* ptr, size_t size) {
  if (ptr == NULL && size == 0) {
    return NULL;
  } else if (ptr == NULL && size > 0) {
    return mm_malloc(size);
  } else if (ptr != NULL && size == 0) {
    mm_free(ptr);
    return NULL;
  }

  mem_block *old_block = find_block(ptr);
  void *new_data = mm_malloc(size);
  memcpy(new_data, old_block->data, size);

  mm_free(ptr);

  return new_data;
}

void mm_free(void* ptr) {
  mem_block *allocated_block = find_block(ptr);
  if (allocated_block != NULL) {
    allocated_block->free = true;
    memset(allocated_block->data, 0, allocated_block->size);

    coalesce_consecutive_free_blocks(allocated_block);
  }
}

/* Combine consecutive adjacent free blocks to form a larger free block. */
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
  if (head->next == tail) {
    head->next = tail->prev = new_block;
    new_block->prev = head;
    new_block->next = tail;
  } else {
    new_block->prev = tail->prev;
    new_block->next = tail;
    tail->prev->next = new_block;
    tail->prev = new_block;
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
  mem_block *first_block = free_block;
  mem_block *second_block = free_block;

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

/* Find the mem_block given its DATA. */
mem_block* find_block(void *data) {
  if (data == NULL) return NULL;

  mem_block *ptr = head->next;
  while (ptr != NULL) {
    if (ptr->data == data) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}
