#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"


struct page 
  {

    void *addr;                 /* virtual address */
    bool read_only;             /* if the page can only be read */
    struct thread *thread;      /* owning thread */
    struct hash_elem hash_elem; /* hash element */
    struct frame *frame;        /* page frame */
    block_sector_t sector;       /* starting sector of swap area, or -1 */
    bool private;               /* if the page is private, 
                                  for noting where to write back */
    struct file *file;          /* the corresponding file */
    off_t file_offset;          /* offset of file */
    off_t file_bytes;           /* bytes to read/write */
  };

struct page *page_allocate (void *, bool read_only);
void page_deallocate (void *vaddr);
bool page_in (void *fault_addr);
bool page_out (struct page *);

bool page_lock (const void *, bool will_write);
void page_unlock (const void *);

void destroy_page (struct hash_elem *p_, void *aux);
hash_hash_func page_hash;
hash_less_func page_less;

#endif /* vm/page.h */
