#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Helper functions for acquiring and releasing locks. */

/* Acquires lock on the thread HOLDER's supplementary page table. */
void
lock_supplement_page_table (struct thread * holder){
 if (LOCK_ERROR)
   printf ("thread %p acquires sptl of %p\n", thread_current (), holder);
 lock_acquire (&holder->supplementary_page_table_lock);
}

/* Releases lock on the thread HOLDER's supplementary page table. */
void
unlock_supplement_page_table (struct thread * holder){
 if (LOCK_ERROR)
   printf ("thread %p releases sptl of %p\n", thread_current (), holder);
 lock_release (&holder->supplementary_page_table_lock);
}

/* Acquires lock on the thread HOLDER's page directory. */
void
lock_pagedir (struct thread * holder){
 if (LOCK_ERROR)
   printf ("thread %p acquires pdl of %p\n", thread_current (), holder);
 lock_acquire (&holder->pagedir_lock);
}

/* Releases lock on the thread HOLDER's page directory. */
void
unlock_pagedir (struct thread * holder){
 if (LOCK_ERROR)
   printf ("thread %p releases pdl of %p\n", thread_current (), holder);
 lock_release (&holder->pagedir_lock);
}
//helper functions to initialization
//1. page_hash
static unsigned
page_hash(const struct hash_elem *elem, void * aux UNUSED){
  const struct page * p = hash_entry(elem, struct page, elem);
  return hash_bytes(&p-> address, sizeof(p->address));
}
//2. cmp_page
static bool
cmp_page (const struct hash_elem *a, const struct hash_elem *b, void * aux UNUSED){
  const struct page * p_a = hash_entry(a, struct page, elem);
  const struct page * p_b = hash_entry(b, struct page, elem);
  return p_a->address < p_b->address;
}

//initialization on supplementary_page_table
//used on process.c, on loading process.
bool
init_supplementary_page_table(struct thread * holder){
  ASSERT (holder !=NULL);
  return hash_init(&holder->supplementary_page_table, page_hash, cmp_page, NULL);
}

//setter function on supplementary_page_table :


// add given page to supplementary_page_table
// in order to safely do this process, need to copy given page on some new struct page
// and then add it on SPT
struct page *
add_supplementary_page(struct page* given_page){
  struct page * add_on = (struct page * )(malloc(sizeof(struct page)));
  struct hash_elem * elem;
  if (add_on ==NULL) return NULL;
  //use on setter fucntion?
  ASSERT (given_page->address < PHYS_BASE);
  add_on->address = given_page->address;
  add_on->offset = given_page->offset;
  add_on->read_bytes = given_page->read_bytes;
  add_on->status = given_page->status;
  add_on->writable = given_page->writable;
  elem = hash_insert(&thread_current()->supplementary_page_table,&add_on->elem);
  return add_on;
}

struct page *
search_supplementary_page(struct thread * holder, void * address){
  struct page * sup_page;
  sup_page->address = address;
  struct hash_elem * target_elem = hash_find(&holder->supplementary_page_table, &sup_page->elem);
  if (target_elem==NULL) return NULL;
  return hash_entry(target_elem, struct page, elem);
}

bool
load_page (struct page *sup_page){
    void * pg_alloc = palloc_get_page(PAL_USER);
    uint32_t modify_offset;
    enum page_status status;
    struct thread * curr = thread_current();
    ASSERT (sup_page->address < PHYS_BASE);
    bool success;
    bool dir_set;
    //success to page allocation
    if (pg_alloc!= NULL){
      success = add_frame(pg_alloc, sup_page->address);
      if (!success){
        palloc_free_page(pg_alloc);
        return false;
      }
    }
    //else, try swap out with eviction
    else{
      //swap out
      //pg must get address
      //so pg = swap_out(arguments)
      //after that...
      //if (pg_alloc == NULL) return false;
    }
    switch(sup_page -> status)
    {
      case IN_MEMORY:
        printf("ERROR ON load_page : non-loaded page is already in memory");
      // load the page from swap, using offset
      case IN_SWAP:
        swap_in(sup_page->offset, pg_alloc);
        break;
      case IN_FILE:
        //use file_read_at function
        int read_bytes_load = file_read_at(curr->executable, pg_alloc, sup_page->read_bytes, sup_page->offset)
        if(read_bytes_load!=(int)sup_page->read_bytes){
          palloc_free_page(pg_alloc);
          return false;
        }
        //make zeros
        memset (pg_alloc + sup_page->read_bytes, 0, PGSIZE-sup_page->read_bytes);
        modify_offset = sup_page->offset;
        break;
      case ON_STACK:
        memset (pg_alloc, 0, PGSIZE);
        break;
      default:
        ASSERT (false);
    }
    sup_page->status = IN_MEMORY;
    sup_page->offset = modify_offset;
    lock_pagedir(curr);
    dir_set = pagedir_set_page(curr->pagedir, address, pg_alloc, sup_page->writterble);
    unlock_pagedir(curr);
    if (!dir_set && pg_alloc != NULL) palloc_free_page(pg_alloc);
    return dir_set;
}
