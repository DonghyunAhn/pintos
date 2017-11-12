#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

enum page_status{IN_MEMORY,IN_SWAP,FROM_FILE,STACK_GROWTH};

struct suppl_pte{

  void *upage;              /* user page address */
  void *kpage;              /* kernel page address. only effective when status = IN_MEMORY */
  enum page_status status;  /* page's status */
  
  /* IN_SWAP */
  uint32_t swap_index;      /* when page is swapped, it indicates swap index. initially -1. */

  /* FROM_FILE */
  struct file * file;       
  bool writable;            /* false : read-only page */
  uint32_t offset;          /* offset of file */
  size_t read_bytes;        /* bytes to read from executables */

  struct hash_elem helem;    /* hash elem for hash mapping */
};

void spt_init(void);
bool spt_allocate(void *upage, void *kpage);
void spt_destroy();
void spt_remove(struct suppl_pte * spte);
bool spt_stakcgrowth(void* upage);
struct suppl_pte * spt_find(struct thread *t ,void *upage);
bool load_page(struct suppl_pte * spte);

#endif /* vm/page.h */
