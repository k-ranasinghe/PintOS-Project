#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/file.h"


struct file_descriptor
{
    struct file *_file;             // pointer to file
    int fd;                         // fid
    struct list_elem fd_elem;       // list elem to add to threads open fds list
};


void syscall_init (void);

#define ERROR -1
#define LOAD_FAIL 2
#define CLOSE_ALL_FD -1
#define NOT_LOADED 0
#define LOADED 1
#define USER_VADDR_BOTTOM ((void *) 0x08048000)

struct child_process {
  int wait;
  int exit;
  int status;
  int pid;
  int load_status;
  struct semaphore load_sema;
  struct semaphore exit_sema;
  struct list_elem elem;
};

struct process_file {
    struct file *file;
    int fd;
    struct list_elem elem;
};

struct lock SystemLock;

#endif /* userprog/syscall.h */