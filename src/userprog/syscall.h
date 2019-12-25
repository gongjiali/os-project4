#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

/* file descriptor */
struct opened_file
  {
    struct list_elem elem; /* list element containing all the opened file */
    struct file *file;     /* the file */
    int handle;            /* id of file */
    struct dir *dir;            /* Directory. */
  };

struct lock file_lock;
void syscall_init (void);
struct mapping
{
    struct list_elem elem;      /* list element */
    int handle;                 /* id */
    struct file *file;          /* the corresponding file. */
    uint8_t *base;              /* base of memory mapping */
    size_t page_cnt;            /* # of pages mapped */
};
void unmap (struct mapping *m);

#endif /* userprog/syscall.h */
