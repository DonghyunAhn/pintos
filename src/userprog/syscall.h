#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int mapid_t;
void syscall_init (void);
void exit(int);
mapid_t mmap(int fd, void * addr);
void munmap(mapid_t mapping);
#endif /* userprog/syscall.h */
