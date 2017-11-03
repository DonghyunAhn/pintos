#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include <stdbool.h>

#define LOCK_ERROR (false)

struct frame{
  void * address;
  struct thread * holder;
  void * vaddr;
  struct list_elem frame_elem;

};

void init_frame(void);
bool add_frame(void *addr, void *vaddr);
void delete_frame(void *addr);
void evict_frame(void *vaddr, struct frame *old);

#endif
