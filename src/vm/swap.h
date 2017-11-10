#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdbool.h>
#include "devices/disk.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"

static void lock_swap();
static void unlock_swap();

void init_swap ();
void swap_free(struct suppl_pte * spte);
void swap_in (disk_sector_t index, void * pg_vaddr);
void swap_out (struct frame_table_entry * evicted);

#endif /* for VM_SWAP_H*/
