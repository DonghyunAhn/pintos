#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "devices/input.h"

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

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);

struct semaphore filesema;

/* Helper function */
struct file_descriptor* fd_to_file(struct thread* t, int fd);

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
  if(!valid_user_addr(file) || get_user(file + initial_size -1) == -1)
    exit(-1);
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
  if(get_user(buffer + size -1) == -1||!valid_user_addr((const uint8_t *)buffer) || !valid_user_addr((const uint8_t *)(buffer + size - 1)))
    exit(-1);
  
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
    struct thread *cur = thread_current();
    
    if(fd_to_file(cur, fd) == NULL){;
      return -1;
    }
      return file_read(fd_to_file(cur,fd)->file, (void *)buffer, (off_t)size); 
  
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

    if(fd_to_file(cur,fd) == NULL)
      return 0;
   
    //print_dw(fd_to_file(cur,fd)->file);
    return file_write(fd_to_file(cur,fd)->file, buffer, size);

  
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
