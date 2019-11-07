#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void close_files(tid_t owner);
void file_lock_acquire(void);
void file_lock_release(void);
#endif /* userprog/syscall.h */
