#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"

void syscall_init (void);
void close_files(tid_t owner);
#endif /* userprog/syscall.h */
