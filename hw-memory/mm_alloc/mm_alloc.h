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

struct mem_block {
    int size;
    bool free;
    struct mem_block *next;
    struct mem_block *prev;
};


void* mm_malloc(size_t size);
void* mm_realloc(void* ptr, size_t size);
void mm_free(void* ptr);

#endif
