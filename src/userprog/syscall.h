#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void close_files(tid_t owner);
void check_user_addr(const void *vaddr);
void deny_lock_acquire(void);
void deny_lock_release(void);
#endif /* userprog/syscall.h */
