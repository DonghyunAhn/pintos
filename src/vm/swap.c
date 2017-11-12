#include "vm/swap.h"
#include "vm/frame.h"
#include "devices/disk.h"
#include "vm/page.h"
#include <bitmap.h>
#include <round.h>
#include <stdio.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

int spp = PGSIZE/DISK_SECTOR_SIZE;
//swap disk
static struct disk * swap_disk;
// Bitmap for manage swap pool
static struct bitmap * swap_pool;
// mutex for swap pool
static struct lock swap_lock;

/* Acquries the swap_lock. */


static void
lock_swap(void){
  lock_acquire(&swap_lock);
}

/* Releases the swap_lock. */
static void
unlock_swap(void){
  lock_release(&swap_lock);
}

void
init_swap(){
  swap_disk = disk_get(1,1); //1:1
  swap_pool = bitmap_create(disk_size(swap_disk)/spp);
  bitmap_set_all(swap_pool, false);
  lock_init(&swap_lock);
}

void
swap_free(struct suppl_pte *spte) {
  //delete partial swap_index on swap_pool
  lock_swap();
  bitmap_reset(swap_pool, spte->swap_index);
  unlock_swap();
}

//copy the swap space with index, to physical address of page
void
swap_in (disk_sector_t sw_index, void * pg){
  ASSERT(pg>=PHYS_BASE); // check page is in kernel
  ASSERT(sw_index < disk_size(swap_disk)/ spp);
  ASSERT (bitmap_test(swap_pool,sw_index) == true) // sw_index indicates unassigned swap disk

  size_t i;
  for (i=0; i<spp;i++){
    disk_read(swap_disk, sw_index*spp+i, pg+i*DISK_SECTOR_SIZE);
  }

  lock_swap();
  //After swap_in, the sw_index should become unassigned index
  bitmap_set(swap_pool,sw_index, false);
  unlock_swap();
}

//call conditinon : when palloc fails
//what to do : select eviction frame, and

// revised version of swap_out
// input : struct frame*, which is decided to be evicted
// need to wrap lock if safty must ensure on swap_pool
// call by eviction?? -> unable to find evicted frame
// if swap_index directly need by this function? -> just change this function to
//swap_index_t swap_out(struct frame * evicted)
void
swap_out(struct frame_table_entry * evicted){
  //defalut(usable) : false, so find first false and flip it
  size_t sw_index;
  lock_swap();
  // give lock before modifying swap_pool
  // now assgined swap index change to true
  sw_index = bitmap_scan_and_flip(swap_pool, 0,1, false);
  unlock_swap();
  evicted->swap = true;
  if(sw_index==BITMAP_ERROR) return;
  struct suppl_pte * sup_page = spt_find(thread_current(),evicted->upage);
  printf("evict : %p\n", evicted->upage);
  sup_page-> status = IN_SWAP;
  sup_page-> swap_index = sw_index;
  size_t i;
  for (i=0;i<spp;i++){
    disk_write(swap_disk, sw_index*spp+i, evicted->fpage+i*DISK_SECTOR_SIZE);
  }
  palloc_free_page(evicted->fpage);
}
