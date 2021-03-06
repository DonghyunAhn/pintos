#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "devices/input.h"
#ifdef VM
#include "vm/page.h"
#include "vm/frame.h"
#endif
static void syscall_handler (struct intr_frame *);

/* address validation */
static bool valid_user_addr (void *esp);

/* syscall function */
void halt (void);
void exit (int status);
tid_t exec (const char *cmd_line);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, const void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);


//added on 3-2
mapid_t mmap(int fd,void *); 
void munmap(mapid_t mmapid);

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
void busy_true(void * buffer, size_t size);
void busy_false(void * buffer, size_t size);
struct semaphore filesema;

/* Helper function */
struct file_descriptor* fd_to_file(struct thread* t, int fd);
struct mmap_descriptor *mapid_to_mmapd(struct thread* t, mapid_t md);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init(&filesema, 1);
}

static bool
valid_user_addr (void *addr)
{
  if(addr == NULL || !is_user_vaddr(addr) || pagedir_get_page(thread_current()->pagedir, addr) == NULL)
    return 0;

  return 1;
}

static void
syscall_handler (struct intr_frame *f) 
{
  if(!valid_user_addr(f->esp) || !valid_user_addr(f->esp + 4) || !valid_user_addr(f->esp + 8) || !valid_user_addr(f->esp + 12))
    exit(-1);
  thread_current()->esp = f->esp;
  int syscall_number;
  syscall_number = *(int *)(f->esp);
  //printf("syscall : %d,  %d\n", syscall_number, thread_current()->tid);
  switch(syscall_number)
  {
    case SYS_HALT:

      halt();
      break;

    case SYS_EXIT:
      exit(*((int *)(f->esp + 4)));
      break;
  
    case SYS_EXEC:

      f->eax = (uint32_t)(exec(*(const char **)(f->esp + 4)));
      break;

    case SYS_WAIT:
      f->eax = (uint32_t)(wait(*(int *)(f->esp + 4)));
      break;

    case SYS_CREATE:
    
      f->eax = (uint32_t)create(*(const char **)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;

    case SYS_REMOVE:
    
      f->eax = (uint32_t)remove(*(const char **)(f->esp + 4));
      break;

    case SYS_OPEN:

      f->eax = (uint32_t)open(*(const char **)(f->esp + 4));
      break;

    case SYS_FILESIZE:
      
      f->eax = (uint32_t)filesize(*(int *)(f->esp + 4));
      break;

    case SYS_READ:

      f->eax = (uint32_t)read(*(int *)(f->esp + 4),*(void **)(f->esp + 8),*(int *)(f->esp + 12));
      break;

    case SYS_WRITE:
      
      f->eax = (uint32_t)write(*(int *)(f->esp + 4), *(void **)(f->esp + 8), *(int *)(f->esp + 12));
      break;

    case SYS_SEEK:

      seek(*(int *)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;

    case SYS_TELL:
      
      f->eax = (uint32_t)tell(*(int *)(f->esp + 4));
      break;

    case SYS_CLOSE:
      
      close(*(int *)(f->esp + 4));
      break;
    case SYS_MMAP:

      f->eax = mmap(*(int *)(f->esp + 4), *(void **)(f->esp +8));
      break;
    case SYS_MUNMAP:
      munmap(*(int*)(f->esp+4));
      break;
  }
  
}
void 
halt (void)
{
  power_off();

}
void 
exit (int status)
{
  struct thread *cur = thread_current();
  struct thread *parent = cur->parent;
  if(!(parent == NULL)){
    struct list * chlist =  &parent->child_list;
    struct list_elem * e;
    struct child_thread* ch;

    if(!list_empty(chlist)){
      for(e = list_tail(chlist); e != list_head(chlist); e = list_prev(e)){
        ch = list_entry(e, struct child_thread, ch_elem);
        if(ch->tid == cur->tid){
          lock_acquire(&parent->loadlock);
          ch->exit_status = status;      
          
          lock_release(&parent->loadlock);

        }
      }
    }
  }
  //printf("%d\n", cur->tid);
  printf("%s: exit(%d)\n",cur->name, status);
  thread_exit();
}

tid_t 
exec (const char *cmd_line)
{
  if(!valid_user_addr(cmd_line) || get_user(cmd_line) == -1)
    exit(-1);
   
  struct thread * cur = thread_current();
  
  sema_down(&cur->loadsema);
  tid_t tid = process_execute(cmd_line);
  //sema_down(&cur->loadsema);
  lock_acquire(&cur->loadlock);
  while(cur->child_exec != 0 && cur->child_exec != 1)
    cond_wait(&cur->loadcond, &cur->loadlock);
  lock_release(&cur->loadlock);
  if(cur->child_exec == 0){
    cur->child_exec = -1;
    tid = -1;
  }
  return tid;
}

int 
wait (tid_t tid)
{
  
  return process_wait(tid);

}

bool 
create (const char *file, unsigned initial_size)
{
  if(file == NULL || get_user(file) == -1){
    exit(-1);
  }

  //sema_down(&filesema);
  bool success = filesys_create(file, initial_size);
  //sema_up(&filesema);
  return success;
}

bool 
remove (const char *file)
{ 
  //sema_down(&filesema);
  bool success = filesys_remove(file);
  //sema_up(&filesema);
  return success;
}

int
open (const char *file)
{
  if(!valid_user_addr(file) || get_user(file) == -1)
    exit(-1);


  struct file_descriptor *file_info = palloc_get_page(0);
  if(!file_info){
    return -1;
  }
  struct file*opened_file = filesys_open(file);
  if(!opened_file){
    palloc_free_page(file_info);
    return -1;    
  }
  
  file_info->file = opened_file;
  struct thread* cur = thread_current();
  if(list_empty(&cur->open_file))
    file_info->fd = 3;
  else
  {
    file_info->fd = (list_entry(list_back(&cur->open_file), struct file_descriptor, fd_elem)->fd) + 1;
  } 
  list_push_back(&cur->open_file, &file_info->fd_elem);
  return file_info->fd;
    
}

int 
filesize (int fd)
{
 struct thread* cur = thread_current();
 struct file* matched_file = fd_to_file(cur, fd)->file;
 return file_length(matched_file);
}

int 
read (int fd, const void *buffer, unsigned size)
{
  if(buffer == NULL || !is_user_vaddr(buffer) || buffer >= PHYS_BASE)
    exit(-1);
  if(get_user(buffer + size -1) == -1||get_user(buffer) == -1){
    exit(-1);
  }
  
  struct thread * cur = thread_current();
  void * buf_pg = pg_round_down(buffer);
  struct suppl_pte *spte = spt_find(cur, buf_pg);
  if(spte != NULL && !spte->writable){
    exit(-1);
  }
  int i;
  uint8_t c;

  if(fd == 0){
  
    for(i=0; i<(int)size; i++)
    {
      c = input_getc();
      if(!put_user(buffer + i, c))
        exit(-1);
    }

    return (int)size;
  
  }
  else
  {
    struct file_descriptor * fdc = fd_to_file(cur, fd); 
    if(fdc == NULL){
      return -1;
    }
    
    if(fdc && fdc->file){
      busy_true(buffer, size);  
      int ret = file_read(fd_to_file(cur,fd)->file, (void *)buffer, (off_t)size); 
      busy_false(buffer, size);
      return ret;
    }
    else
      return -1;
  }

}

int 
write (int fd, const void *buffer, unsigned size)
{
  if(!valid_user_addr((const uint8_t *)buffer) || !valid_user_addr((const uint8_t *)(buffer + size -1)))
    exit(-1);
  if(fd == 1){
    putbuf(buffer, size);
    return (int) size;
  }

  else{
    struct thread *cur = thread_current();
    struct file_descriptor * fdc = fd_to_file(cur, fd);
    if(fdc == NULL)
      return 0;
   
    //print_dw(fd_to_file(cur,fd)->file);
    if(fdc && fdc->file){
    busy_true(buffer, size);
    int ret = file_write(fd_to_file(cur,fd)->file, buffer, size);
    busy_false(buffer, size);
    
    return ret;
    }
    else
      return 0;
  }

}
void 
seek (int fd, unsigned position)
{
  struct thread *cur = thread_current();
  //sema_down(&filesema);
  file_seek(fd_to_file(cur, fd)->file, position);
  //sema_down(&filesema);

}
unsigned 
tell (int fd)
{
  struct thread* cur = thread_current();
  //sema_down(&filesema);
  return file_tell(fd_to_file(cur, fd)->file);
  //sema_up(&filesema);
}
void 
close (int fd)
{
  struct thread* cur = thread_current();
  if(fd_to_file(cur, fd) == NULL)
    return;
  file_close(fd_to_file(cur, fd)->file);
  list_remove(&fd_to_file(cur, fd)->fd_elem);


}
mapid_t mmap(int fd, void * addr){
  mapid_t err;
  err = ((mapid_t) -1);

  //3-2 implementation
  if (addr ==NULL || pg_ofs(addr)||fd<=1) goto ERROR;
  struct thread * cur = thread_current();
  struct file *f = NULL;
  struct file_descriptor * file_d = fd_to_file(cur,fd);
  //if (file_d->dir !=NULL) goto ERROR;//file_d should be NULL -> ERROR
  if (file_d==NULL || file_d->file==NULL) goto ERROR;
  f = file_reopen(file_d->file);
  if (f==NULL) goto ERROR;
  size_t file_size = file_length(f);
  if(file_size==0) goto ERROR;

  //mapping filesystem <-> page
  //check all the page addr's are not using
  size_t offset;
  for (offset=0;offset<file_size;offset+=PGSIZE){
    void * temp_addr = addr+offset;
    //one line?
    if(spt_find(cur,temp_addr)!=NULL) goto ERROR;
  }

  for (offset =0; offset<file_size;offset+=PGSIZE){
    void * temp_addr = addr+offset;
    size_t read_byte = PGSIZE;
    if (offset+PGSIZE>=file_size) read_byte = file_size-offset;
    //from here
    struct suppl_pte *spte;
    spte = (struct suppl_pte *) malloc(sizeof(struct suppl_pte));
    spte->upage = temp_addr;
    spte->kpage = NULL;
    spte->status = FROM_FILE;
    spte->file = f;
    spte->offset = offset;
    spte->read_bytes = read_byte;
    spte->writable = true;
    spte->fte = NULL;
    //change it
    struct hash_elem *e;
    e = hash_insert(&cur->suppl_page_table, &spte->helem);
  }

  //assign it
  mapid_t mid=1;
  if (!list_empty(&cur->mmap_list))
		mid += list_entry(list_back(&cur->mmap_list), struct mmap_descriptor, elem);

  struct mmap_descriptor *mmap_d = (struct mmap_descriptor *) malloc(sizeof(struct mmap_descriptor));
  mmap_d -> id =mid;
  mmap_d -> file =f;
  mmap_d -> addr = addr;
  mmap_d -> size = file_size;
  list_push_back(&cur->mmap_list, &mmap_d->elem);
  return mid;

  ERROR:
  return err;
}


void munmap(mapid_t mapping){

  struct thread * cur = thread_current();
  struct mmap_descriptor * mmap_d = mapid_to_mmapd(cur, mapping);

  ASSERT(mmap_d != NULL);
  size_t offset;
  size_t file_size = mmap_d->size;
  for(offset=0; offset<file_size; offset+=PGSIZE){

    size_t file_bytes;
    if(offset + PGSIZE < file_size){
      file_bytes = PGSIZE;
    }
    else
      file_bytes = file_size - offset;

    void *addr = mmap_d->addr + offset;
    struct suppl_pte * spte = spt_find(cur, addr);
    uint32_t * pagedir  = cur->pagedir;

    ASSERT(spte != NULL);
    if(spte->status == IN_MEMORY)
      frame_set_busy(spte->fte);

    void * temp;
    switch(spte->status)
    {
      case IN_MEMORY: // need discussion on is_dirty
        if(pagedir_is_dirty(pagedir,spte->upage)){
          file_write_at(mmap_d->file, spte->upage, file_bytes, offset);
        }
        frame_remove(spte->fte);
        spte->fte = NULL;
        pagedir_clear_page(pagedir, spte->upage);
        break;

      case IN_SWAP:
        if(pagedir_is_dirty(pagedir, spte->upage)){
          temp = palloc_get_page(0);
          swap_in(spte->swap_index, temp);
          file_write_at(mmap_d->file, temp, file_bytes, offset);
          palloc_free_page(temp);

        }
        else
          swap_free(spte);
				break;
      case FROM_FILE:
        break;
      case STACK_GROWTH:
        break;
      default:
        ASSERT(false);
    }

    hash_delete(&cur->suppl_page_table, &spte->helem);
  }

  list_remove(&mmap_d->elem);
  file_close(mmap_d->file);
  free(mmap_d);
}


struct mmap_descriptor *mapid_to_mmapd(struct thread* t, mapid_t md){
  if(list_empty(&t->mmap_list))
    return NULL;
  struct list_elem *e;
  for(e = list_head(&t->mmap_list); e != list_tail(&t->mmap_list); e = list_next(e)){

    struct mmap_descriptor * matched = list_entry(e, struct mmap_descriptor, elem);
    if(matched->id == md){

      return matched;
    }


  }
  return NULL;
}				


struct file_descriptor *fd_to_file(struct thread* t, int fd)
{
  if(list_empty(&t->open_file))
    return NULL;
  struct list fdlist = t->open_file;
  struct list_elem * e;
  for(e = list_head(&fdlist); e != list_tail(&fdlist); e = list_next(e)){
    
    struct file_descriptor *matched = list_entry(e, struct file_descriptor, fd_elem);
    if(matched->fd == fd){
      return matched;
    
    }
  
  }

  return NULL;

}

static int 
get_user(const uint8_t *uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result) : "m" (*uaddr));
    return result;
}

static bool 
put_user (uint8_t *udst, uint8_t byte) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst) : "q" (byte));
    return error_code != -1;
}

void busy_true(void * buffer, size_t size){

  void * upage;
  for(upage = pg_round_down(buffer); upage < buffer + size; upage += PGSIZE){
    
    struct suppl_pte * spte = spt_find(thread_current(),upage);
    load_page(spte);
    page_set_busy(thread_current(), upage);
    
  }

}

void busy_false(void * buffer, size_t size){

  void * upage;

  for(upage = pg_round_down(buffer);upage < buffer + size; upage += PGSIZE){
    
    page_set_unbusy(thread_current(), upage);
  }


}
