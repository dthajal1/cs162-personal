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

int main() {
  load_alloc_functions();

  // int* data = mm_malloc(sizeof(int));
  // assert(data != NULL);
  // data[0] = 0x162;
  // mm_free(data);
  // puts("malloc test successful!");

  puts("testing reuse free memory");
  reuse_free_memory();
  puts("reuse free memory passes!");
}
