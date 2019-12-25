#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

#define STACK_MAX (1024 * 1024)


void
destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  if (p->frame){
    /* releases frame f */
    lock_acquire(&(p->frame)->lock);
    p->frame->page = NULL;
    lock_release (&(p->frame)->lock);
  }
  free (p);
}


/* looking for the page according to the address */
static struct page *
page_for_addr (const void *address)
{
  if (address >= PHYS_BASE)
    return NULL;
    
  struct page p;
  struct hash_elem *e;
  /* get virtual address */
  p.addr = (void *) pg_round_down (address);
  /* find its hash elem */
  e = hash_find (thread_current ()->pages, &p.hash_elem);
  if (e)
    return hash_entry (e, struct page, hash_elem);

  /* need to access the stack & address meet the restriction of the
   stack => allocate one more stack page */
  if ((p.addr > PHYS_BASE - STACK_MAX) && 
        ((void *)thread_current()->user_esp - 32 < address))
    return page_allocate (p.addr, false);

  return NULL;
}




/* page in */
bool page_in (void *fault_addr)
{
  struct page *p;

  /* cannot handle page faults without a hash table */
  if (thread_current ()->pages == NULL)
    return false;

  p = page_for_addr (fault_addr);
  if (p == NULL)
    return false;

  if (p->frame)
  {
    lock_acquire(&(p->frame)->lock);
  }
  else
  {
      /* allocate a frame for the page. */
      p->frame = frame_alloc_and_lock (p);
      /* allocate failed */
      if (p->frame == NULL)
        return false;

      /* copy data into frame */
      if (p->sector != (block_sector_t) -1)
        /* get data by swapping */
        swap_in (p);
      else if (p->file != NULL)
        {
        /* get data from file */
          off_t read_bytes = file_read_at (p->file, p->frame->base,
                                        p->file_bytes, p->file_offset);
          off_t zero_bytes = PGSIZE - read_bytes;
          memset (p->frame->base + read_bytes, 0, zero_bytes);
        }
      else
        /* all-zero */
        memset (p->frame->base, 0, PGSIZE);

    }

  /* put into page table */
  bool ret = pagedir_set_page (thread_current ()->pagedir, p->addr,
                              p->frame->base, !p->read_only);
  lock_release (&(p->frame)->lock);

  return ret;
}


bool
page_out (struct page *p) 
{
  pagedir_clear_page(p->thread->pagedir, (void *) p->addr);
  bool dirty = pagedir_is_dirty (p->thread->pagedir, (const void *) p->addr);
  bool ret = !dirty;
  
  if (p->file == NULL)
    ret = swap_out(p);
  else
  {
    if (dirty)/* if the page has been modified */
    {
      if(p->private)
        ret = swap_out(p);
      else
        /* write back to memory*/
        ret = file_write_at(p->file, (const void *) p->frame->base,
                             p->file_bytes, p->file_offset);
    }
  }

  if(ret)
    p->frame = NULL;
  return ret;
}

struct page *
page_allocate (void *vaddr, bool read_only)
{
  struct page *p = malloc (sizeof *p);
  if (p)
    {
      /* initialization */
      p->addr = pg_round_down (vaddr);
      p->read_only = read_only;
      p->private = !read_only;
      p->frame = NULL;
      p->sector = (block_sector_t) -1;
      p->file = NULL;
      p->file_offset = 0;
      p->file_bytes = 0;
      p->thread = thread_current ();
      /* insert it into the list */
      if (hash_insert (thread_current ()->pages, &p->hash_elem) != NULL)
        {
          free (p);
          p = NULL;
        }
    }
  return p;
}


/* deallocate the page */
void page_deallocate (void *vaddr)
{
  struct page *p = page_for_addr (vaddr);
  struct frame *f = p->frame;
  /* if page is in frame, free it */
  if (f)
    {
      lock_acquire(&(p->frame)->lock);
      if (p->file && !p->private)
        page_out (p);
      /* releases frame f */
      f->page = NULL;
      lock_release (&f->lock);
    }

  /* delete hash elem in hash list*/
  hash_delete (thread_current ()->pages, &p->hash_elem);
  free (p);
}


unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

/* compare the address of two pages */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}

/* lock the page into physical memory */
bool page_lock (const void *addr, bool to_be_written)
{
  struct page *p = page_for_addr (addr);
  /* cannot be locked or do not need to be locked */
  if (p == NULL || (p->read_only && to_be_written))
    return false;

  if (p->frame)
    {
      /* lock the frame */
      lock_acquire(&(p->frame)->lock);
      return true;
    }
  else
    {
      /* if the page is not in the physical memory, page in and then lock */
      bool ret = page_in (addr); 
      lock_acquire(&(p->frame)->lock);
      return ret;
    }
}

/* unlock the page */
void page_unlock (const void *addr)
{
  struct page *p = page_for_addr (addr);
  lock_release (&(p->frame)->lock);
}
