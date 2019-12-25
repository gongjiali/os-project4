#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"


/* # sectors in the page */
#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap_device;

/* used swap pages */
struct bitmap *swap_bitmap;

/* for swap_bitmap. */
struct lock swap_lock;


/* initialization */
void swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  lock_init (&swap_lock);
  if (swap_device)
    swap_bitmap = bitmap_create (block_size (swap_device) / PAGE_SECTORS);
  else
    swap_bitmap = bitmap_create (0);
  
}


/* swap in page P */
void swap_in (struct page *p)
{
  for (size_t i = 0; i < PAGE_SECTORS; i++)
    block_read (swap_device, p->sector + i,
                p->frame->base + i * BLOCK_SECTOR_SIZE);
  bitmap_reset (swap_bitmap, p->sector / PAGE_SECTORS);
  p->sector = (block_sector_t) -1;
}


/* swap out page p */
bool
swap_out (struct page *p)
{
  lock_acquire (&swap_lock);
  size_t slot = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
  lock_release (&swap_lock);
  if (slot == BITMAP_ERROR)
    return false;

  p->sector = slot * PAGE_SECTORS;

  for (size_t i = 0; i < PAGE_SECTORS; i++)
    block_write (swap_device, p->sector + i,
              (uint8_t *) p->frame->base + i * BLOCK_SECTOR_SIZE);
  
  p->private = false;
  p->file = NULL;
  p->file_offset = 0;
  p->file_bytes = 0;
  return true;
}
