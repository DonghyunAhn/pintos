#include <hash.h>
#include <string.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"

/* hash helper functions */

static unsigned spte_hash(const struct hash_elem * elem, void *aux){
  struct suppl_pte *spte = hash_entry(elem, struct suppl_pte, helem);
  return hash_int((int)spte->upage);
}

static bool spte_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
  struct suppl_pte *sa = hash_entry(a, struct suppl_pte, helem);
  struct suppl_pte *sb = hash_entry(b, struct suppl_pte, helem);
  return sa->upage < sb->upage;
}

struct suppl_pte * spt_find(struct thread *t, void * upage);

static void spte_destroy_func(struct hash_elem *elem, void *aux UNUSED){

  struct suppl_pte *spte = hash_entry(elem, struct suppl_pte, helem);

  // remove associated frame
  if (spte->kpage != NULL) {
    frame_remove(spte->upage);
  }
  else if(spte->status == IN_SWAP) {
    swap_free (spte);
  }
  
  // free SPTE.
  free (spte);
}


void
spt_init(){

  hash_init(&thread_current()->suppl_page_table, spte_hash, spte_less, NULL);

}

bool
spt_allocate(void *upage, void *kpage){
 
  //printf("s : %p\n", upage);
  ASSERT(upage != NULL && kpage != NULL);
  struct suppl_pte * spte = malloc(sizeof(struct suppl_pte));
  spte->upage = upage;
  spte->kpage = kpage;
  spte->status = IN_MEMORY;
  spte->swap_index = -1;
  
  struct thread * cur = thread_current();
  struct hash_elem * prev;
  prev = hash_insert(&cur->suppl_page_table, &spte->helem);
  if(prev == NULL)
    return true;
  else
    return false;   /* already exists */

}

struct suppl_pte *
spt_stackgrowth(void *upage){
  
  //printf("s : %p\n", upage);
  struct suppl_pte * spte = malloc (sizeof(struct suppl_pte));
  if(!spte) return false;
  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = STACK_GROWTH;
  spte->swap_index = -1;
  spte->writable = true;

  struct thread * cur = thread_current();
  struct hash_elem * prev;
  prev = hash_insert(&cur->suppl_page_table, &spte->helem);
  if (prev==NULL)
    return spte;
  else
    return NULL;
}

void 
spt_destroy(){

  hash_destroy(&thread_current()->suppl_page_table, spte_destroy_func);

}

void
spt_remove(struct suppl_pte * spte){
  ASSERT(spte != NULL);
  struct thread *cur = thread_current();
  struct hash_elem *e = hash_delete(&cur->suppl_page_table, &spte->helem);
  free(spte);


}


struct suppl_pte * 
spt_find(struct thread * t, void *upage){

  struct suppl_pte temp;
  temp.upage = upage;
  struct hash_elem * e = hash_find(&t->suppl_page_table, &temp.helem);
  if(e == NULL)
    return NULL;
  return hash_entry(e, struct suppl_pte, helem);
}

/* Loads a page that is not in memory yet, from the given SPTE that must be 
 * already in the supplementary page table of the current thread.
 * Returns true if suceeded. */
bool
load_page(struct suppl_pte * spte){

  ASSERT(spte != NULL);
  bool writable = true;
  struct thread * cur = thread_current();
  if(spte->status == IN_MEMORY)
    return true;

  void * frame = frame_allocate(spte->upage, PAL_USER);
  if(frame == NULL)
    return false;

  switch(spte->status)
  {
    case IN_MEMORY:
      ASSERT(false);
      break;
    case IN_SWAP:
      swap_in(spte->swap_index, frame);
      frame_find(spte->upage)->swap = false;
      break;
    case FROM_FILE:
      file_seek(spte->file, spte->offset);
      if((int)spte->read_bytes > file_read(spte->file, frame , spte->read_bytes)){
        frame_remove(spte->upage);
        return false;
      }
      memset(frame+spte->read_bytes, 0, PGSIZE-spte->read_bytes);
      writable = spte->writable;
      break;
    case STACK_GROWTH:
      memset(frame, 0, PGSIZE);
      break;
    default:
      ASSERT(false);
  }
  if(!pagedir_set_page(cur->pagedir, spte->upage, frame , writable)){
    return false;
  }

  spte->status = IN_MEMORY;
  spte->writable = writable; 
          
  return true; 
}
