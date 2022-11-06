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
  size_t size = 50;
  char *data = mm_malloc(size);
  data[0] = 'H';
  data[49] = 'W';
  mm_free(data);
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
  size_t size = 10;
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
  char *data2 = mm_malloc(size + 5);
  char *data3 = mm_malloc(size + 10);
  data1[0] = 'H';
  data1[size - 1] = 'W';
  data2[0] = 'H';
  data2[size + 4] = 'W';
  data3[0] = 'H';
  data3[size + 9] = 'W';
  mm_free(data1);
  mm_free(data2);
  mm_free(data3);

  size_t sum_sizes = size * 3 + 10 + 5;
  puts("hello wrold!");
  char *data4 = mm_malloc(sum_sizes);
  puts("hello wrold!");
  for (size_t i = 0; i < sum_sizes; i++) {
    printf("%d\n", i);
    assert(data4[i] == 0);
  }
  puts("hello wrold!");
  mm_free(data4);
  puts("hello wrold!");
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

  // puts("testing coalesce freed memory");
  // coalesce_freed_memory();
  // puts("coalesce freed memory passes!");

  puts("testing coalesce multiple freed memory");
  coalesce_multiple_freed_memory();
  puts("coalesce multiple freed memory passes!");

  // puts("testing realloc");
  // test_realloc();
  // puts("realloc passes!");
}
