#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include "devices/disk.h"
#include "threads/thread.h"
#include "vm/page.h"

void init_swap (void);
void delete_swap(struct page * spg);
void swap_in (disk_sector_t index, void * pg_vaddr);


#endif /* for VM_SWAP_H*/
