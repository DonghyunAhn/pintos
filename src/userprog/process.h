#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "userprog/syscall.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void arg_stack (char **parse, int token , void **esp);

struct file_descriptor
{
  int fd;
  struct file* file;
  struct list_elem fd_elem;

};
struct mmap_descriptor
{
  mapid_t id; 
  struct list_elem elem;
  struct file* file;

  void *addr;
  size_t size;
};

#endif /* userprog/process.h */
