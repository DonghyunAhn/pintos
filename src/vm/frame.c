#include "vm/frame.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"

// list of physical frames in memory
// elem as elem in struct frame
static struct list frame_table;

// mutex lock for frame
static struct lock frame_mutex;

// current element of issued frame
static struct list_elem * issued_frame;

//Initialize frame_table, frame_mutex. Called on threads/init.c
void
init_frame(void){
  list_init(&frame_table);
  issued_frame = list_end(&frame_table);
  lock_init(&frame_mutex);
}

// same as lock_acquire(&frame_mutex);, but check for LOCK ERROR
static void
lock_frame(void){
  if(LOCK_ERROR) printf("thread %p acquire frame error\n", thread_current());
  lock_acquire(&frame_mutex);
}

// same as lock_release(&frame_mutex);, but check for LOCK ERROR
static void
unlock_frame(void){
  if(LOCK_ERROR) printf("thread %p releases frame error\n", thread_current());
  lock_release(&frame_mutex);
}

// Add information of frame at kernel virtual address
// vaddr is corresponding user virtual address.
bool
add_frame(void * addr, void * vaddr){
  struct frame * fr = (struct frame *)(malloc (sizeof)(struct frame));
  if (fr==NULL) return false;
  lock_frame();
  fr->address = addr;
  fr->holder = thread_current();
  fr->vaddr = vaddr;
  list_push_front(&frame_table, &fr->elem);
  unlock_frame();
  return true;
}

void
delete_frame(void * addr) {
  ASSERT(address!=NULL);
  lock_frame();
  struct list_elem * elem;
  struct frame * fr;
  fr = NULL;
  for (elem=list_begin(&frame_table); elem!=list_end(&frame_table);elem = list_next(elem)){
    struct frame * fr_sub = list_entry(elem, struct frame, frame_elem);
    if(fr_sub->address == addr){
      // before removeing, move issued_frame for maintainance.
      if(issued_frame==elem) issued_frame = list_remove(elem);
      else list_remove(elem);
      fr = fr_sub;
      break;
    }
  }
  if (fr!=NULL)
    free(fr);
  unlock_frame();
}
// eviction
void evict_frame(void * vaddr, struct frame *old){
  lock_frame();
  struct frame * victim;
  struct thread * cur = thread_current();
  bool flag = true;
  while(flag){
    //second chance algorithm
    issued_frame = list_begin(&frame_table);
    for(;issued_frame!= list_end(&frame_table); issued_frame = list_next(issued_frame)){
      victim = list_entry(issued_frame, struct frame, frame_elem);
      lock_pagedir(victim->holder);
      if(pagedir_is_accessed(victim->holder->pagedir, victim->vaddr)){
        pagedir_set_accessed(victim->holder->pagedir, victim->vaddr, false);
      }
      else{
        unlock_pagedir(victim->holder);
        // when victim->holder == cur, then lock already be held
        if (victim->holder != cur) lock_supplement_page_table(victim->holder);
        *old = *victim;
        victim->holder = cur;
        victim->vaddr = vaddr;
        flag = false;
        break;
      }
      unlock_pagedir(victim->holder);
    }
    if (!flag) break;
  }
  unlock_frame();
}
