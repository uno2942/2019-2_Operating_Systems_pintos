#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void close_files(tid_t owner);
void file_lock_acquire(void);
void file_lock_release(void);
bool is_file_lock_held (void);
void clear_mmap_list_for_exit(void);
#endif /* userprog/syscall.h */
