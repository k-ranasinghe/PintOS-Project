# Pintos Project

This repository contains my implementation of the Pintos operating system for semester 3 Operating Systems module. Pintos is a teaching OS for x86 architectures, developed by Stanford University, and used in academic settings to explore core OS concepts. As part of this project, I implemented several key features, including Threads, User Programs, Argument Passing, and System Calls.

## Overview

Pintos is a minimal operating system that includes all the basic functionalities of an OS. The project was carried out as part of the Semester 3 Operating Systems Module. Throughout the project, I implemented features like argument passing, system calls, process scheduling, and concurrency mechanisms like semaphores and locks. The implementation was done on a Linux environment since Pintos cannot be run on Windows or MacOS.

### Lab 1: Interactive Shell

In Lab 1, I developed an interactive shell as a part of the project. This shell allows users to interact with the Pintos kernel and execute basic commands. Below are some screenshots of the shell in action:

![Interactive Shell](https://github.com/user-attachments/assets/1e3546d2-70be-44af-b23c-16af361c4af8)

![Shell Commands](https://github.com/user-attachments/assets/68e8591e-dd38-4c24-b36b-800f6547e25c)

### Lab 2: User Programs

Lab 2 focused on implementing User Programs. This involved the development of argument passing, system calls, process scheduling, and setting up the memory layout for user programs. To verify the correctness and robustness of the implementation, a set of automated tests were run:

![Automated Tests](https://github.com/user-attachments/assets/1b856be4-9fe4-4952-8a87-70991ab00824)

![Test Results](https://github.com/user-attachments/assets/27151c17-dc10-45ed-9792-611ea9a519f7)

## Argument Passing

### Data Structures

I did not introduce any new data structures or modify existing ones specifically for argument passing.

### Implementation

Argument passing was implemented using the `strtok_r()` function, which splits the command string into the program name and arguments. These elements are passed to the `thread_create` function, where they are pushed onto the stack in the correct order during the process's initialization.

### Rationale

Pintos uses `strtok_r()` instead of `strtok()` to ensure thread safety. Unlike `strtok()`, which uses a static pointer, `strtok_r()` is reentrant, making it safe for use in a multi-threaded environment.

### Unix vs. Pintos Argument Separation

1. **Modularity**: Separating arguments from the executable name allows for a more modular design, where programs can interact through standardized interfaces, making integration and maintenance easier.
2. **Flexibility**: Users can modify program behavior by passing different arguments without altering the executable itself.

## System Calls

### Data Structures

#### File Descriptors
```c
struct file_descriptor {
   struct file *file;  // Pointer to the open file
   int fd;             // File descriptor number
};
```
Represents a process's file descriptor entry, containing details about a file and its associated file descriptor number.
```c
struct file_descriptor file_descriptors[MAX_FILES];
```
An array mapping opened files to file descriptor numbers within a process.

### Implementation

#### File Descriptors

File descriptors are unique within each process but not across the entire OS. The `file_descriptors` array maps file descriptors to open files within a process.

#### Read/Write Implementation

Read: The `read` system call retrieves the corresponding `struct file` pointer using the file descriptor. For `STDIN_FILENO`, `input_getc()` is used. For other descriptors, `file_read_at` is used, and data is copied from the kernel buffer to the user buffer.

Write: The `write` system call retrieves the `struct file` pointer using the file descriptor. For `STDOUT_FILENO`, `putbuf` writes directly to the console. For other descriptors, `file_write_at` is used.

### Page Table Inspections

- 4,096 bytes: If contiguous, one inspection; if scattered, up to 4,096 inspections.
- 2 bytes: One or two inspections, depending on contiguity.
- Improvements: Using bulk copying mechanisms like `memcpy` or contiguous memory allocations can reduce the number of inspections.

### `wait` System Call

The wait system call synchronizes parent and child processes using a semaphore (`load_sema`). The parent process waits on this semaphore until the child process either completes loading or exits, ensuring proper synchronization and resource cleanup.

### Error Handling

Centralized error-checking functions validate user pointers before memory access. Resources like locks and buffers are managed through a cleanup process that ensures they are released if an error occurs.

## Synchronization

### `exec` System Call

The `exec` system call uses a semaphore to block the parent process until the child has successfully loaded the new executable. The semaphore ensures that the parent process only continues if the loading is successful.

### Handling Race Conditions

Proper synchronization is ensured by using semaphores and data structures. The parent process blocks on a semaphore until the child exits, ensuring that resources are correctly freed regardless of whether the parent waits or terminates before the child.

## Conclusion

This project provided an in-depth understanding of operating systems, from basic kernel operations to advanced concepts like system calls and synchronization. The Pintos environment, although challenging, offered a realistic platform for learning and implementing key OS features.
