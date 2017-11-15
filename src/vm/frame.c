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
struct frame_table_entry  *
frame_allocate(void* upage , enum palloc_flags flag){
 
  lock_acquire(&frame_lock);
  struct frame_table_entry * fte = malloc(sizeof(struct frame_table_entry));
  void * frame = palloc_get_page(flag);
  /* if physical memory is full, evict some frame */
  if(frame == NULL){
    //printf("up : %d, %p\n", thread_current()->tid,upage);
    frame_evict();
    frame = palloc_get_page(PAL_USER | flag);
  }
  fte->holder = thread_current();
  fte->fpage = frame;
  fte->upage = upage;
  fte->swap = false;
  fte->busy = true;
  /* insert fte to the frame table */
  list_push_back(&frame_table, &fte->elem);
  lock_release(&frame_lock);
  return fte;
}

void
frame_remove(struct frame_table_entry * fte){
  
  lock_acquire(&frame_lock);

  ASSERT(fte->fpage != NULL);
  if(!fte->swap)
    list_remove(&fte->elem);

  free(fte);
   
  lock_release(&frame_lock);
}

/* find fte from user page address */
struct frame_table_entry *frame_find(void * kpage){

  struct list_elem * e = list_front(&frame_table);
  struct frame_table_entry * fte;
  while(e  != list_back(&frame_table)){
  
    fte = list_entry(e, struct frame_table_entry, elem);
    if(fte->fpage == kpage){
      //printf("gg\n");
      break;
    }
    e = list_next(e);
  }

  ASSERT(fte != NULL);
  return fte;
}

/* choose the frame by second chance algorithm for eviction */
void
frame_evict(){
  
  void * new_frame;
  struct list_elem * e = list_begin(&frame_table);
  struct frame_table_entry * victim = list_entry(e, struct frame_table_entry, elem);
  struct thread * cur = thread_current();
  /* second chance algorithm */
  
  
  while ((victim->busy) && (victim->fpage == NULL || pagedir_is_accessed(victim->holder->pagedir, victim->upage))){
  
    if(victim->fpage != NULL)
      pagedir_set_accessed(victim->holder->pagedir, victim->upage, false);
    list_remove(e);
    list_push_back(&frame_table, e);
    e = list_begin(&frame_table);
    victim = list_entry(e, struct frame_table_entry , elem);
  }
  //printf("victim : %d, %p, %p\n", cur->tid, victim->upage, victim->fpage);
  
  pagedir_clear_page(victim->holder->pagedir, victim->upage);
  swap_out(victim);
  list_remove(&victim->elem);
  free(victim);
}

void
frame_set_busy(struct frame_table_entry *frame){

  lock_acquire(&frame_lock);

  frame->busy = true;

  lock_release(&frame_lock);
}
void
frame_set_unbusy(struct frame_table_entry *frame){

  lock_acquire(&frame_lock);

  frame->busy = false;

  lock_release(&frame_lock);
}
