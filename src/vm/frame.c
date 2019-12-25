#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static struct frame *frames;
static size_t frame_cnt;

static struct lock scan_lock;
static size_t hand;


/* initialization */
void frame_init (void) 
{
  void *base;

  lock_init (&scan_lock);
  /* allocate a frame */
  frames = malloc (sizeof *frames * init_ram_pages);

  while ((base = palloc_get_page (PAL_USER)) != NULL) 
    {
      struct frame *f = &frames[frame_cnt++];
      lock_init (&f->lock);
      f->base = base;
      f->page = NULL;
    }
}



struct frame *
try_frame_alloc_and_lock (struct page *page) 
{

  lock_acquire (&scan_lock);

  /* first, try to find an empty frame */
  for (size_t i = 0; i < frame_cnt; i++)
    {
      struct frame *f = &frames[i];
      if (!lock_try_acquire (&f->lock))
        continue;
      /* found */
      if (f->page == NULL) 
        {
          f->page = page;
          lock_release (&scan_lock);
          return f;
        } 
      lock_release (&f->lock);
    }

  /* if cannot find a free frame */
  for (size_t i = 0; i < frame_cnt * 2; i++) 
    {
      struct frame *f = &frames[hand];
      /* try again */
      if (++hand >= frame_cnt)
        hand = 0;

      if (!lock_try_acquire (&f->lock))
        continue;
      /* to see if a page is accessed recently */
      if (pagedir_is_accessed (f->page->thread->pagedir, f->page->addr))
        {
          pagedir_set_accessed (f->page->thread->pagedir, f->page->addr, false);
          lock_release (&f->lock);
          continue;
        }
          
      lock_release (&scan_lock);
      
      if (!page_out (f->page))
        {
          lock_release (&f->lock);
          return NULL;
        }

      /* not locked & not accessed recently => evict this frame. */
      f->page = page;
      return f;
    }

  /* not found */
  lock_release (&scan_lock);
  return NULL;
}


/* allocate and lock a frame */
struct frame *
frame_alloc_and_lock (struct page *page) 
{
  for (size_t i = 0; i < 3; i++) 
    {
      struct frame *f = try_frame_alloc_and_lock (page);
      if (f)
        return f; 
      timer_msleep (1000);
    }

  return NULL;
}
