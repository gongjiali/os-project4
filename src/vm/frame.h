#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

/*  physical frame. */
struct frame 
  {
    /* prevent simultaneous access */
    struct lock lock;
    /* kernel virtual base address */
    void *base;
    /* mapped process page */
    struct page *page;
  };

void frame_init (void);
struct frame * try_frame_alloc_and_lock (struct page *page);
struct frame *frame_alloc_and_lock (struct page *);

#endif /* vm/frame.h */
