#include <hash.h>
#include <list.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

static struct list frame_table; /* frame table. */
static struct lock frame_lock; /* for synchronization of managing frame table */

struct frame_table_entry *frame_find(void * upage);
void
frame_init(){
  
	list_init(&frame_table);
	lock_init(&frame_lock);	

}

/* allocate the frame and add it to the frame table. */
/* if allocation successes, return frame address. */
void *
frame_allocate(void* upage , enum palloc_flags flag){
  
  //printf("f : %p\n", upage);
  struct frame_table_entry * fte = malloc(sizeof(struct frame_table_entry));
  void * frame = palloc_get_page(flag);
  /* if physical memory is full, evict some frame */
  while(frame == NULL){
    frame_evict();
    frame = palloc_get_page(PAL_USER | flag);
  }
  fte->holder = thread_current();
  fte->fpage = frame;
  fte->upage = upage;
  fte->swap = -1;
  
  /* insert fte to the frame table */
  lock_acquire(&frame_lock);
  list_push_back(&frame_table, &fte->elem);
  lock_release(&frame_lock);
  return frame;
}

void
frame_remove(void * upage){
  
  struct frame_table_entry * fte = frame_find(upage);
  lock_acquire(&frame_lock);

  ASSERT(fte->fpage != NULL);
  if(!fte->swap)
    list_remove(&fte->elem);
  palloc_free_page(fte->fpage);
  pagedir_clear_page(fte->holder->pagedir, fte->upage);
  free(fte);
  
  lock_release(&frame_lock);
}

/* find fte from user page address */
struct frame_table_entry *frame_find(void * upage){

  struct list_elem * e = list_front(&frame_table);
  struct frame_table_entry * fte;
  while(e  != list_back(&frame_table)){
  
    fte = list_entry(e, struct frame_table_entry, elem);
    if(fte->upage == upage)
      break;
    e = list_next(e);
  }

  ASSERT(fte != NULL);
  return fte;
}

/* choose the frame by second chance algorithm for eviction */
void
frame_evict(){
  
  lock_acquire(&frame_lock);
  void * new_frame;
  struct list_elem * e = list_front(&frame_table);
  struct frame_table_entry * victim = list_entry(e, struct frame_table_entry, elem);

  /* second chance algorithm */
  while(victim->fpage == NULL || pagedir_is_accessed(victim->holder->pagedir, victim->upage)){
  
    if(victim->fpage != NULL)
      pagedir_set_accessed(victim->holder->pagedir, victim->upage, false);
    list_remove(e);
    list_push_back(&frame_table, e);
    e = list_front(&frame_table);
    victim = list_entry(e, struct frame_table_entry , elem);
  }
  list_remove(&victim->elem);
  swap_out(victim);
  lock_release(&frame_lock);
}

