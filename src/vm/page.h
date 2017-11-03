#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "threads/thread.h"

//for page status, enum would be useful
enum page_status{IN_MEMORY, IN_SWAP, IN_FILE, ON_STACK};

struct page{
  void * address;
  enum page_status status;
  bool writable; // true when page is writterble
  uint32_t offset; // offset will indicate file_offset for read_only pages, and swap indexes for writables
  size_t read_bytes; //bytes to read from executables.
  struct hash_elem elem; // need to hash mapping
};
bool init_supplementary_page_table(struct thread * holder);
void lock_suppl_page_table (struct thread * holder);
void lock_pagedir (struct thread * holder);
void unlock_suppl_page_table (struct thread * holder);
void unlock_pagedir (struct thread * holder);
struct page * add_supplementary_page(struct page * given_page);
struct page * search_supplementary_page(struct thread * holder, void * address);

#endif /*for VM_PAGE_H*/
