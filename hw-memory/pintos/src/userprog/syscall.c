#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include <round.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void syscall_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void validate_buffer_in_user_region(const void* buffer, size_t length) {
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr(buffer) || length > delta)
    syscall_exit(-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void validate_string_in_user_region(const char* string) {
  uintptr_t delta = PHYS_BASE - (const void*)string;
  if (!is_user_vaddr(string) || strnlen(string, delta) == delta)
    syscall_exit(-1);
}

static int syscall_open(const char* filename) {
  struct thread* t = thread_current();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open(filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int syscall_write(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  } else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_write(t->open_file, buffer, size);
}

static int syscall_read(int fd, void* buffer, unsigned size) {
  struct thread* t = thread_current();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int)file_read(t->open_file, buffer, size);
}

static void syscall_close(int fd) {
  struct thread* t = thread_current();
  if (fd == 2 && t->open_file != NULL) {
    file_close(t->open_file);
    t->open_file = NULL;
  }
}

/* Allocate pages and map them into the user’s virtual address space if necessary.
  Page allocation necessary if incrementing current thread's heap_brk by INCREMENT 
  crosses already allocated page boundary. */
static bool allocate_and_map(intptr_t increment) {
  struct thread* t = thread_current();

  uint8_t* allocated_pg_boundry = pg_round_up((void *)t->heap_brk);
  if ((t->heap_brk + increment) > allocated_pg_boundry) {
    size_t num_pages = DIV_ROUND_UP(increment, PGSIZE);
    uint8_t* kpages = palloc_get_multiple(PAL_USER | PAL_ZERO, num_pages);
    if (kpages != NULL) {
      uint8_t* kpage_offset = kpages;
      uint8_t* upage_offset = allocated_pg_boundry;
      while (num_pages != 0) {
        // if (pagedir_get_page(t->pagedir, allocated_pg_boundry) == NULL) { // do i need to check this?
        if (!pagedir_set_page(t->pagedir, upage_offset, kpage_offset, true)) {
          palloc_free_multiple(kpages, num_pages);
          return false;
        }
        kpage_offset += PGSIZE;
        upage_offset += PGSIZE;
        num_pages -= 1;
      }
    } else {
      return false;
    }
  }
  return true;
}

/* Deallocate pages that no longer contain part of the heap as necessary. 
  Page deallocation necessary if decrementing current thread's heap_brk by DECREMENT
  crosses the base of already allocated page boundary. */
static bool deallocate_unused_pages(intptr_t decrement) {
  struct thread* t = thread_current();

  uint8_t* allocated_pg_boundry_base = pg_round_down((void *)t->heap_brk);
  if ((t->heap_brk + decrement) <= allocated_pg_boundry_base) {
    size_t num_pages = (decrement * -1) / PGSIZE;
    uint8_t* upage_offset = allocated_pg_boundry_base;
    while (num_pages != 0) {
      pagedir_clear_page(t->pagedir, upage_offset);
      upage_offset -= PGSIZE;
      num_pages -= 1;
    }
    // uint8_t* first_pages = upage_offset;
    uint8_t* first_pages = upage_offset;
    palloc_free_multiple(first_pages, num_pages);

    // uint8_t* kpage = pagedir_get_page(t->pagedir, allocated_pg_boundry_base);
    // if (kpage == NULL) {
    //   return false;
    // }
    // pagedir_clear_page(t->pagedir, allocated_pg_boundry_base);
    // palloc_free_page(kpage);
  }
  return true;
}

static void* syscall_sbrk(intptr_t increment) {
  struct thread* t = thread_current();

  // can't go below the start of the heap
  if ((t->heap_brk + increment) < t->heap_start) {
    return (void*) -1;
  }

  bool is_success = false;
  if (increment == 0) {
    return t->heap_brk;
  } else if (increment > 0) {
    is_success = allocate_and_map(increment);
  } else if (increment < 0) {
    is_success = deallocate_unused_pages(increment);
  }

  if (is_success) {
    uint8_t* old_brk = t->heap_brk;
    t->heap_brk += increment;
    return (void *) old_brk;
  }

  return (void*) -1;
}

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = (uint32_t*)f->esp;
  struct thread* t = thread_current();
  t->in_syscall = true;

  validate_buffer_in_user_region(args, sizeof(uint32_t));
  switch (args[0]) {
    case SYS_EXIT:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_exit((int)args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      validate_string_in_user_region((char*)args[1]);
      f->eax = (uint32_t)syscall_open((char*)args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region(&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)syscall_read((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region(&args[1], sizeof(uint32_t));
      syscall_close((int)args[1]);
      break;

    case SYS_SBRK:
      validate_buffer_in_user_region(&args[1], sizeof(intptr_t));
      f->eax = (uint32_t)syscall_sbrk((intptr_t) args[1]);
      break;

    default:
      printf("Unimplemented system call: %d\n", (int)args[0]);
      break;
  }

  t->in_syscall = false;
}
