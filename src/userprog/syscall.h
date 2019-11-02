#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void close_files(tid_t owner);
void check_user_addr(const void *vaddr);

#endif /* userprog/syscall.h */
