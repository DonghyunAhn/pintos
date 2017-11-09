#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include <list.h>
#include <stdbool.h>
#include "threads/palloc.h"


/* frame table entry structure */
struct frame_table_entry{
  
  struct thread * holder;   /* holder thread */
  
	void * fpage;             /* frame page address */
  void * upage;             /* user page address */

  struct list_elem elem;    /* elem for hash table */
	bool swap;						/* boolean for swap */
};

/* basic functions */
void frame_init(void);
void * frame_allocate(void * upage , enum palloc_flags flag);
void frame_remove(struct frame_table_entry * fte);
struct frame_table_entry * frame_find(void * upage);
void frame_evict(void);

#endif /* vm/frame.h */
