/*
 * mm_alloc.h
 *
 * Exports a clone of the interface documented in "man 3 malloc".
 */

#pragma once

#ifndef _malloc_H_
#define _malloc_H_

#include <stdlib.h>
#include <stdbool.h>

typedef struct mem_block {
    bool free;
    struct mem_block *next; 
    struct mem_block *prev; 
    size_t size;
    char data[];  /* pointer to the allocated data on heap */
    /* https://en.wikipedia.org/wiki/Flexible_array_member */
} mem_block;


void* mm_malloc(size_t size);
void* mm_realloc(void* ptr, size_t size);
void mm_free(void* ptr);

#endif
