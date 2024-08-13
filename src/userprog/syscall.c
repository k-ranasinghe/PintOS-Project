#include "userprog/syscall.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"

#define CONSOLE_OUTPUT 1
#define ERROR_STATUS -1
#define KEYBOARD_INPUT 0

static void syscall_handler(struct intr_frame *);
static void exit(int status);
static tid_t exec(const char *cmd_args);
static int wait(tid_t tid);
static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned size);
static int write(int fd, const void *buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);

void BufferValidate(const void *buffer, unsigned size);
int *get_kth_ptr(const void *_ptr, int _k);
struct file_descriptor *get_from_fd(int fd);
void ptrValidate(const void *_ptr);
void strValidate(const char *_str);

void syscall_init(void)
{
  // Initialize the file system lock
  lock_init(&SystemLock);
  // Register the system call interrupt handler
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// Handler for system calls
static void syscall_handler(struct intr_frame *f UNUSED)
{
  // Validate the pointer
  ptrValidate(f->esp);
  // Extract the system call type
  int syscall_type = *get_kth_ptr(f->esp, 0);

  // Perform actions based on the system call type
  switch (syscall_type)
  {
  case SYS_HALT:
  {
    shutdown_power_off();
    break;
  }

  case SYS_EXIT:
  {
    int status = *get_kth_ptr(f->esp, 1);
    exit(status);
    break;
  }

  case SYS_EXEC:
  {
    char *cmd_args = *(char **)get_kth_ptr(f->esp, 1);
    strValidate(cmd_args);
    f->eax = exec(cmd_args);
    break;
  }

  case SYS_WAIT:
  {
    tid_t tid = *get_kth_ptr(f->esp, 1);
    f->eax = wait(tid);
    break;
  }

  case SYS_CREATE:
  {
    char *file = *(char **)get_kth_ptr(f->esp, 1);
    strValidate(file);
    unsigned initial_size = *((unsigned *)get_kth_ptr(f->esp, 2));
    f->eax = create(file, initial_size);
    break;
  }

  case SYS_REMOVE:
  {
    char *file = *(char **)get_kth_ptr(f->esp, 1);
    strValidate(file);
    f->eax = remove(file);
    break;
  }

  case SYS_OPEN:
  {
    char *file = *(char **)get_kth_ptr(f->esp, 1);
    strValidate(file);
    f->eax = open(file);
    break;
  }

  case SYS_FILESIZE:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    f->eax = filesize(fd);
    break;
  }

  case SYS_READ:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    void *buffer = (void *)*get_kth_ptr(f->esp, 2);
    unsigned size = *((unsigned *)get_kth_ptr(f->esp, 3));
    BufferValidate(buffer, size);

    f->eax = read(fd, buffer, size);
    break;
  }

  case SYS_WRITE:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    void *buffer = (void *)*get_kth_ptr(f->esp, 2);
    unsigned size = *((unsigned *)get_kth_ptr(f->esp, 3));
    BufferValidate(buffer, size);

    f->eax = write(fd, buffer, size);
    break;
  }

  case SYS_SEEK:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    unsigned position = *((unsigned *)get_kth_ptr(f->esp, 2));
    seek(fd, position);
    break;
  }

  case SYS_TELL:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    f->eax = tell(fd);
    break;
  }

  case SYS_CLOSE:
  {
    int fd = *get_kth_ptr(f->esp, 1);
    close(fd);
    break;
  }

  default:
  {
    break;
  }
  }
}


// Sets the exit status of the current thread and exits the thread
static void exit(int status)
{
  struct thread *thr = thread_current();
  thr->exit_status = status;
  thread_exit();
}

// Function to handle the exec syscall
static tid_t exec(const char *cmd_args)
{
  // Get the current thread
  struct thread *current_thread = thread_current();
  struct thread *child_thread;
  struct list_elem *child_elem;

  tid_t child_tid = process_execute(cmd_args);

  if (child_tid == TID_ERROR)
  {
    return child_tid;
  }

  // Check if the child_tid is present in the current thread's list of children
  for (child_elem = list_begin(&current_thread->child_list); child_elem != list_end(&current_thread->child_list); child_elem = list_next(child_elem))
  {
    child_thread = list_entry(child_elem, struct thread, child_elem);
    if (child_thread->tid == child_tid)
    {
      break;
    }
  }

  if (child_elem == list_end(&current_thread->child_list))
  {
    return ERROR_STATUS;
  }

  // Wait for the child process to initialize
  sema_down(&child_thread->init_sema);

  // Check if the child's status loading was successful
  if (!child_thread->status_load_success)
  {
    return ERROR_STATUS;
  }

  // Return the child_tid upon successful execution
  return child_tid;
}

// Function to handle the wait syscall
static int wait(tid_t tid)
{
  // Use the process_wait function from the process module to wait for the child process
  return process_wait(tid);
}

// Function to handle the create syscall
static bool create(const char *filename, unsigned initial_size)
{
  // Acquire the file system lock before accessing the file system
  lock_acquire(&SystemLock);

  // Call filesys_create to create a new file with the given filename and initial size
  bool create_success = filesys_create(filename, initial_size);

  // Release the file system lock after the file creation operation
  lock_release(&SystemLock);

  // Return the status of the file creation operation
  return create_success;
}

// Function to handle the remove syscall
static bool remove(const char *filename)
{
  // Acquire the file system lock before accessing the file system
  lock_acquire(&SystemLock);
  bool remove_success = filesys_remove(filename);
  lock_release(&SystemLock);

  // Return the status of the file removal operation
  return remove_success;
}

// Function to handle the open syscall
static int open(const char *filename)
{
  // Allocate memory for the file descriptor
  struct file_descriptor *fd = malloc(sizeof(struct file_descriptor *));

  // Create variables for the file and the current thread
  struct file *file_ptr;
  struct thread *current_thread;

  lock_acquire(&SystemLock);
  file_ptr = filesys_open(filename);
  lock_release(&SystemLock);

  // If the file could not be opened, return the error status
  if (file_ptr == NULL)
  {
    return ERROR_STATUS;
  }

  // Get the current thread
  current_thread = thread_current();

  // Set the file descriptor and increment the next file descriptor count
  fd->fd = current_thread->next_fd;
  current_thread->next_fd++; 
  fd->_file = file_ptr;

  // Add the file descriptor to the list of open file descriptors
  list_push_back(&current_thread->open_fd_list, &fd->fd_elem);

  // Return the file descriptor
  return fd->fd;
}

// Function to handle the filesize syscall
static int filesize(int fd)
{
  // Get the file descriptor from the file descriptor list using the provided file descriptor value
  struct file_descriptor *fd_struct = get_from_fd(fd);
  int size_of_file;

  // If the file descriptor is not found, return the error status
  if (fd_struct == NULL)
  {
    return ERROR_STATUS;
  }

  // Acquire the file system lock before performing the file operation
  lock_acquire(&SystemLock);
  size_of_file = file_length(fd_struct->_file);
  lock_release(&SystemLock);

  // Return the size of the file
  return size_of_file;
}

// Function to handle the read syscall
static int read(int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int bytes_read = 0;

  if (fd == KEYBOARD_INPUT)
  {
    // Read data from the keyboard input and store it in the buffer
    for (unsigned i = 0; i < size; i++)
    {
      *((uint8_t *)buffer + i) = input_getc();
      bytes_read++;
    }
  }
  else if (fd == CONSOLE_OUTPUT)
  {
    // Return error status if attempting to read from console output
    return ERROR_STATUS;
  }
  else
  {
    // Get the file descriptor from the file descriptor list using the provided file descriptor value
    fd_struct = get_from_fd(fd);

    // If the file descriptor is not found, return the error status
    if (fd_struct == NULL)
    {
      return ERROR_STATUS;
    }

    // Acquire the file system lock before performing the file read operation
    lock_acquire(&SystemLock);
    bytes_read = file_read(fd_struct->_file, buffer, size);
    lock_release(&SystemLock);
  }

  // Return the number of bytes read
  return bytes_read;
}

// Function to handle the write syscall
static int write(int fd, const void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  char *_buffer = (char *)buffer;
  int bytes_written = 0;

  if (fd == CONSOLE_OUTPUT)
  {
    // Write the buffer contents to the console output
    putbuf(_buffer, size);
    bytes_written = size;
  }
  else if (fd == KEYBOARD_INPUT)
  {
    // Return error status if attempting to write to keyboard input
    return ERROR_STATUS;
  }
  else
  {
    // Get the file descriptor from the file descriptor list using the provided file descriptor value
    fd_struct = get_from_fd(fd);

    // If the file descriptor is not found, return the error status
    if (fd_struct == NULL)
    {
      return ERROR_STATUS;
    }

    // Acquire the file system lock before performing the file write operation
    lock_acquire(&SystemLock);
    bytes_written = file_write(fd_struct->_file, _buffer, size);
    lock_release(&SystemLock);
  }

  // Return the number of bytes written
  return bytes_written;
}

// Function to handle the seek syscall
static void seek(int fd, unsigned position)
{
  struct file_descriptor *fd_struct = get_from_fd(fd);

  // If the file descriptor is found, acquire the file system lock before performing the seek operation
  if (fd_struct != NULL)
  {
    lock_acquire(&SystemLock);
    file_seek(fd_struct->_file, position);
    lock_release(&SystemLock);
  }
}

// Function to handle the tell syscall
static unsigned tell(int fd)
{
  unsigned position = 0;
  struct file_descriptor *fd_struct = get_from_fd(fd);

  // If the file descriptor is not found, return the current position (0) without performing any operations
  if (fd_struct == NULL)
  {
    return position;
  }

  // Acquire the file system lock before retrieving the position and release it afterwards
  lock_acquire(&SystemLock);
  position = file_tell(fd_struct->_file);
  lock_release(&SystemLock);

  return position;
}

// Function to handle the close syscall
static void close(int fd)
{
  struct file_descriptor *fd_struct = get_from_fd(fd);

  // Check if the file descriptor exists
  if (fd_struct != NULL)
  {
    // Acquire the file system lock before closing the file and release it afterwards
    lock_acquire(&SystemLock);
    file_close(fd_struct->_file);
    lock_release(&SystemLock);

    // Remove the file descriptor element from the list and deallocate the memory
    list_remove(&fd_struct->fd_elem);
    free(fd_struct);
  }
}

// Function to validate the pointer
void ptrValidate(const void *_ptr)
{
  struct thread *curr_t = thread_current();

  // Check if the pointer is NULL
  if (_ptr == NULL)
  {
    // The pointer should not be NULL
    exit(ERROR_STATUS);
  }

  // Check if the pointer is in kernel address space
  if (is_kernel_vaddr(_ptr))
  {
    // The pointer should not be in kernel address space
    exit(ERROR_STATUS);
  }

  // Check if the page corresponding to the pointer is mapped
  if (pagedir_get_page(curr_t->pagedir, _ptr) == NULL)
  {
    // The address should be mapped
    exit(ERROR_STATUS);
  }
}

// Function to validate a string
void strValidate(const char *_str)
{
  // Validate the base pointer of the string
  ptrValidate((void *)_str);
  for (int k = 0; *((char *)_str + k) != 0; k++)
  {
    // Validate each character in the string
    ptrValidate((void *)((char *)_str + k + 1));
  }
}

// Function to validate a buffer
void BufferValidate(const void *buffer, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
  {
    // Validate each element in the buffer
    ptrValidate((void *)((char *)buffer + i));
  }
}

// Function to get the pointer at an offset 'k' from the given pointer '_ptr'
int *get_kth_ptr(const void *_ptr, int _k)
{
  // Compute the pointer at the specified offset 'k'
  int *next_ptr = (int *)_ptr + _k;
  ptrValidate((void *)next_ptr);
  ptrValidate((void *)(next_ptr + 1));

  return next_ptr;
}

// Function to retrieve a file descriptor from the current process' list of open file descriptors using
struct file_descriptor *get_from_fd(int fd)
{
  // Get the current thread
  struct thread *curr_t = thread_current();
  struct file_descriptor *_file_descriptor;
  struct list_elem *fd_elem;

  // Iterate through the list of open file descriptors to find the one with matching 'fd'
  for (
      fd_elem = list_begin(&curr_t->open_fd_list);
      fd_elem != list_end(&curr_t->open_fd_list);
      fd_elem = list_next(fd_elem))
  {
    // Extract the file descriptor from the list element
    _file_descriptor = list_entry(fd_elem, struct file_descriptor, fd_elem);
    if (_file_descriptor->fd == fd)
    {
      break;
    }
  }
  // If 'fd' was not found in the list, return NULL
  if (fd_elem == list_end(&curr_t->open_fd_list))
  {
    return NULL;
  }

  // Return the file descriptor pointer
  return _file_descriptor;
}
