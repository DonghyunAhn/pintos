#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <bitmap.h>
#include <round.h>
#include <stdio.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"


#define PAGE_SIZE_IN_SECTORS (DIV_ROUND_UP (PGSIZE, DISK_SECTOR_SIZE))
//swap disk
static struct disk * swap_disk;
// Bitmap for manage swap pool
static struct bitmap * swap_pool;
// mutex for swap pool
static struct lock swap_lock;

/* Acquries the swap_lock. */


static void
lock_swap (void){
  if (LOCK_ERROR)
    printf ("thread %p acquires swap\n", thread_current ());
  lock_acquire (&swap_lock);
}

/* Releases the swap_lock. */
static void
unlock_swap (void){
  if (LOCK_ERROR)
    printf ("thread %p releases swap\n", thread_current ());
  lock_release (&swap_lock);
}

void
init_swap(){
  swap_disk = disk_get(1,1); //1:1
  swap_pool = bitmap_create(disk_size(swap_disk)/ PAGE_SIZE_IN_SECTORS);
  bitmap_set_all (swap_pool, false);
  lock_init(&swap_lock);
}

void
delete_swap(struct page *spg) {
  lock_swap();
  bitmap_reset(swap_pool, spg->offset);
  unlock_swap();
}

//copy the swap space with index, to virtual address of page
void
swap_in (disk_sector_t index, void * pg_vaddr){
  size_t i;
  for (i=0; i<PAGE_SIZE_IN_SECTORS;i++){
    disk_read(swap_disk, index*PAGE_SIZE_IN_SECTORS+i, pg_vaddr+i*DISK_SECTOR_SIZE);
  }
  lock_swap();
  //After swap_in, need to mark empty on swap pool
  bitmap_reset(swap_pool,index);
  unlock_swap();
}

void * swap_out(void * vaddr){};
