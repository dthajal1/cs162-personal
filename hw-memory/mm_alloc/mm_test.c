#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

static void* try_dlsym(void* handle, const char* symbol) {
  char* error;
  void* function = dlsym(handle, symbol);
  if ((error = dlerror())) {
    fprintf(stderr, "%s\n", error);
    exit(EXIT_FAILURE);
  }
  return function;
}

static void load_alloc_functions() {
  void* handle = dlopen("hw3lib.so", RTLD_NOW);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  mm_malloc = try_dlsym(handle, "mm_malloc");
  mm_realloc = try_dlsym(handle, "mm_realloc");
  mm_free = try_dlsym(handle, "mm_free");
}

static void reuse_free_memory() {
  size_t size = 10000;
  char *data = mm_malloc(size);
  mm_free(data);
  char *data2 = mm_malloc(size);
  for (size_t i = 0; i < size; i++) {
    assert(data[i] == 0);
  }
}

static void split_large_blocks() {
  size_t size = 1000000;
  char *data1 = mm_malloc(size);
  char *data2 = mm_malloc(size);
  mm_free(data1);
  
  char *data3 = mm_malloc(size / 2);
  char *data4 = mm_malloc(size / 2);

  mm_free(data2);
  mm_free(data3);
  mm_free(data4);
}

static void coalesce_freed_memory() {
  size_t size = 100000;
  char *data1 = mm_malloc(size);
  char *data2 = mm_malloc(size);
  mm_free(data1);
  mm_free(data2);
  char *data3 = mm_malloc(size * 2);
  mm_free(data3);
}

static void coalesce_multiple_freed_memory() {
  size_t size = 1000;
  char *data1 = mm_malloc(size);
  for (size_t i = 0; i < size; i++) {
    data1[i] = 'H';
  }
  char *data2 = mm_malloc(size * 2);
  for (size_t i = 0; i < size + 2; i++) {
    data2[i] = 'E';
  }
  char *data3 = mm_malloc(size * 3);
  for (size_t i = 0; i < size * 3; i++) {
    data3[i] = 'L';
  }

  mm_free(data1);
  mm_free(data2);
  mm_free(data3);

  char *data4 = mm_malloc(size + size * 2 + size * 3);
  for (size_t i = 0; i < size + size * 2 + size * 3; i++) {
    assert(data4[i] == 0);
  }
  mm_free(data4);
}

void test_realloc() {
  size_t size = 10;
  char *data1 = mm_malloc(size);
  data1[0] = 'H';
  data1[9] = 'W';
  char *data2 = mm_realloc(data1, size * 2);
  assert(data2[0] == 'H');
  assert(data2[9] == 'W');
  assert(data2[10] == 0);
}

int main() {
  load_alloc_functions();

  // int* data = mm_malloc(sizeof(int));
  // assert(data != NULL);
  // data[0] = 0x162;
  // mm_free(data);
  // puts("malloc test successful!");

  // puts("testing reuse free memory");
  // reuse_free_memory();
  // puts("reuse free memory passes!");

  // puts("testing split large blocks");
  // split_large_blocks();
  // puts("split large blocks passes!");

  puts("testing coalesce freed memory");
  coalesce_freed_memory();
  puts("coalesce freed memory passes!");

  // puts("testing coalesce multiple freed memory");
  // coalesce_multiple_freed_memory();
  // puts("coalesce multiple freed memory passes!");

  // puts("testing realloc");
  // test_realloc();
  // puts("realloc passes!");
}
