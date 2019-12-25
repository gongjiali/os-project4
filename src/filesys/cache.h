#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Type of block lock. */
enum lock_type 
  {
    NON_EXCLUSIVE,	/* Any number of lockers. */
    EXCLUSIVE		/* Only one locker. */
  };


struct cache_block 
  {
    struct lock block_lock;                    /* used while dealing with cache blocks */
    struct condition no_readers_or_writers; /* if readers == 0 && writers == 0 */
    struct condition no_writers;         /* if writers == 0 */
    int readers, read_waiters;          /* # of readers, # waiting to read. */
    int writers, write_waiters; /* # of writers (<= 1), # waiting to write. */

    block_sector_t sector;    /* number of sectors */
    bool up_to_date;    /* check correctness */
    bool dirty;         /* if dirty, need to write back */
    struct lock data_lock;              /* used while checking data correctness */
    uint8_t data[BLOCK_SECTOR_SIZE];    /* data on disk */
  };

struct readahead_block 
  {
    struct list_elem list_elem;         /* readahead_list element. */
    block_sector_t sector;              /* Sector to read. */
  };

void cache_init (void);
void cache_flush (void);
struct cache_block *cache_lock (block_sector_t, enum lock_type);
void *cache_read (struct cache_block *);
void cache_unlock (struct cache_block *);
void cache_free (block_sector_t);
void readahead_submit (block_sector_t sector);

#endif /* filesys/cache.h */
