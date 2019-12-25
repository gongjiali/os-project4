#include "filesys/cache.h"
#include <debug.h>
#include <string.h>
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define INVALID_SECTOR ((block_sector_t) -1)
#define CACHE_CNT 64


struct cache_block cache[CACHE_CNT];/* cache data */
struct lock cache__lock;/* used while allocating and evicting */
static int write_hand = 0; /* cache write_hand */
static struct condition nonreadahead;
static struct list readahead_list;
static struct lock readahead_lock;
static void write_back_init (void);
static void readahead_init (void);
static void write_back (void *aux);
static void readahead (void *aux);



/* Initializes cache. */
void
cache_init (void) 
{
  /* for every cache block */
  for (int i = 0; i < CACHE_CNT; i++) 
    {
      struct cache_block *b = &cache[i];
      lock_init (&b->block_lock);
      cond_init (&b->no_readers_or_writers);
      cond_init (&b->no_writers);
      b->readers = 0;
      b->read_waiters = 0;
      b->writers = 0;
      b->write_waiters = 0;
      b->sector = INVALID_SECTOR;
      lock_init (&b->data_lock);
    }
  lock_init (&cache__lock);
  thread_create ("write_back", PRI_MIN, write_back, NULL);
  readahead_init ();
}

static void
write_back (void *aux UNUSED) 
{
  while(true)
    {
      timer_msleep (30 * 1000);
      cache_write_back ();
    }
}


/* put dirty cache onto disk. */
void
cache_write_back (void) 
{
  for (int i = 0; i < CACHE_CNT; i++)
    {
      struct cache_block *b = &cache[i];
      
      /* need a lock to check if the sector can be accessed */
      lock_acquire (&b->block_lock);
      block_sector_t sector = b->sector;
      lock_release (&b->block_lock);

      /* skip the invalid sectors */
      if (sector == INVALID_SECTOR)
        continue;

      /* put on the disk */
      b = cache_lock (sector, EXCLUSIVE);
      if (b->up_to_date && b->dirty) 
        {
          block_write (fs_device, b->sector, b->data);
          b->dirty = false; 
        }
      cache_unlock (b);
    }
}



/* read data from cache */
void *
cache_read (struct cache_block *b) 
{
  lock_acquire (&b->data_lock);
  if (!b->up_to_date) 
    {
      block_read (fs_device, b->sector, b->data);
      b->up_to_date = true;
      b->dirty = false; 
    }
  lock_release (&b->data_lock);
  return b->data;
}


static struct cache_block *
cache_try_lock(block_sector_t sector, enum lock_type type);


struct cache_block *
cache_lock (block_sector_t sector, enum lock_type type) 
{
 struct cache_block * ret;
 ret = NULL;
 do{
   lock_acquire (&cache__lock);
   ret = cache_try_lock(sector, type);
   /* lock_release (&cache__lock); */
  
 }while (ret == NULL);
  return ret;
}



static struct cache_block *
cache_try_lock(block_sector_t sector, enum lock_type type)
 {
  for (int i = 0; i < CACHE_CNT; i++)
    {
      struct cache_block *b = &cache[i];
      lock_acquire (&b->block_lock);
      /* block found in cache */
      if (b->sector == sector) 
        {
          lock_release (&cache__lock);

          if (type == NON_EXCLUSIVE) /* just read */
            {
              b->read_waiters++;
              /* if there is a writer or write waiter, let it wait */
              if (b->writers || b->write_waiters)
                do {
                  cond_wait (&b->no_writers, &b->block_lock);
                } while (b->writers);
              
              /* now read */
              b->readers++;
              b->read_waiters--;
            }
          else /* write */
            {
              b->write_waiters++;
              /* if there is a reader or read waiter or writer, let it wait */
              if (b->readers || b->read_waiters || b->writers)
                do {
                  cond_wait (&b->no_readers_or_writers, &b->block_lock);
                } while (b->readers || b->writers);
              
              /* now write */
              b->writers++;
              b->write_waiters--;
            }
          lock_release (&b->block_lock);
          return b;
        }
      lock_release (&b->block_lock);
    }

  /* not in cache, evict an empty invalid sector */
  for (int i = 0; i < CACHE_CNT; i++)
    {
      struct cache_block *b = &cache[i];
      lock_acquire (&b->block_lock);
      if (b->sector == INVALID_SECTOR) 
        { 
          lock_release (&b->block_lock);
          b->sector = sector;
          b->up_to_date = false;
          if (type == NON_EXCLUSIVE)
            b->readers = 1;
          else
            b->writers = 1;
          lock_release (&cache__lock);
          return b;
        }
      lock_release (&b->block_lock); 
    }

  /* no empty sector, find a sector to evict */
  for (int i = 0; i < CACHE_CNT; i++)
    {
      struct cache_block *b = &cache[write_hand];
      write_hand++;
      if (write_hand >= CACHE_CNT)
        write_hand = 0;

      lock_acquire (&b->block_lock);
      /* there is read or write requirement */
      if (b->readers || b->writers || b->read_waiters || b->write_waiters) 
        {
          lock_release (&b->block_lock);
          continue;
        }

      /* no read or write requirement, evict */
      b->writers = 1;
      lock_release (&b->block_lock);
      lock_release (&cache__lock);

      if (b->up_to_date && b->dirty) 
        {
          block_write (fs_device, b->sector, b->data);
          b->dirty = false;
        }

      lock_acquire (&b->block_lock);
      b->writers = 0;
      if (!b->read_waiters && !b->write_waiters) 
        {
          b->sector = INVALID_SECTOR; 
          /* lock_release (&b->block_lock);
          b->sector = sector;
          b->up_to_date = false;
          if (type == NON_EXCLUSIVE)
            b->readers = 1;
          else
            b->writers = 1;
          lock_release (&cache__lock);
          return b; */
        }
      else 
        {
          if (b->read_waiters)
            cond_broadcast (&b->no_writers, &b->block_lock);
          else
            cond_signal (&b->no_readers_or_writers, &b->block_lock);
        }
      lock_release (&b->block_lock);

      return NULL;
    }
    /* wait some time and try again */
    timer_msleep (1000);
    lock_release (&cache__lock);
    return NULL;
}








/* unlocks a block when some reading or writing work is done*/
void
cache_unlock (struct cache_block *b) 
{
  lock_acquire (&b->block_lock);
  /* consider readers first */
  if (b->readers) 
    {
      b->readers--;
      if (b->readers == 0)
        cond_signal (&b->no_readers_or_writers, &b->block_lock);
    }
    /* then writers */
  else if (b->writers)
    {
      b->writers--;
      /* there is some readers waiting to read, reader first */
      if (b->read_waiters)
        cond_broadcast (&b->no_writers, &b->block_lock);
      else
        cond_signal (&b->no_readers_or_writers, &b->block_lock);
    }
  lock_release (&b->block_lock);
}


void
cache_free (block_sector_t sector) 
{
  lock_acquire (&cache__lock);
  for (int i = 0; i < CACHE_CNT; i++)
    {
      struct cache_block *b = &cache[i];

      lock_acquire (&b->block_lock);
      if (b->sector == sector) 
        { 
          if (b->readers == 0 && b->read_waiters == 0
              && b->writers == 0 && b->write_waiters == 0) 
            b->sector = INVALID_SECTOR; 
          lock_release (&b->block_lock);
          break;
        }
      lock_release (&b->block_lock);
    }
  lock_release (&cache__lock);
}




static void
readahead_init (void) 
{
  lock_init (&readahead_lock);
  cond_init (&nonreadahead);
  list_init (&readahead_list);
  thread_create ("readahead", PRI_MIN, readahead, NULL);
}

/* add the job to read ahead queue */
void
readahead_submit (block_sector_t sector) 
{
  struct readahead_block *block = malloc (sizeof *block);
  if (block)
    {
      block->sector = sector;
      /* push to the list */
      lock_acquire (&readahead_lock);
      list_push_back (&readahead_list, &block->list_elem);
      cond_signal (&nonreadahead, &readahead_lock);
      lock_release (&readahead_lock);
    }
}


static void
readahead (void *aux UNUSED) 
{
  while(true)
    {
      lock_acquire (&readahead_lock);
      while (list_empty (&readahead_list)) 
        cond_wait (&nonreadahead, &readahead_lock);
      struct readahead_block *ra_b = list_entry (list_pop_front (&readahead_list),
                             struct readahead_block, list_elem);
      lock_release (&readahead_lock);

      struct cache_block *cache_b = cache_lock (ra_b->sector, NON_EXCLUSIVE);
      cache_read (cache_b);
      cache_unlock (cache_b);
      free (ra_b);
    }
}
