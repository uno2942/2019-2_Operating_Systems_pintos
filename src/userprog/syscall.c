#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/interrupt.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include <console.h>

static int fd_count;

static struct list fd_list;

struct file_descriptor{
    struct file* file;
    int fd;
    struct list_elem elem;
};

bool fd_less(const struct list_elem *a, const struct list_elem *b, void *aux);
struct file* find_file(int fd);

void syscall_init (void);

void exit_handle (struct intr_frame *f, int status);
void exec_handle (struct intr_frame *f, const char *file);
void wait_handle (struct intr_frame *f, tid_t pid);
void create_handle (struct intr_frame *f, const char *file, unsigned initial_size);
void remove_handle (struct intr_frame *f, const char *file);
void open_handle (struct intr_frame *f, const char *file);
void filesize_handle (struct intr_frame *f, int fd);
void read_handle (struct intr_frame *f, int fd, void *buffer, unsigned size);
void write_handle (struct intr_frame *f, int fd, const void *buffer, unsigned size);
void seek_handle (struct intr_frame *f, int fd, unsigned position);
void tell_handle (struct intr_frame *f, int fd);
void close_handle (struct intr_frame *f, int fd);

static void syscall_handler (struct intr_frame *);


bool
fd_less(const struct list_elem *a, const struct list_elem *b, void *aux){
  struct file_descriptor* fa = list_entry (a, struct file_descriptor, elem);
  struct file_descriptor* fb = list_entry (b, struct file_descriptor, elem);
  if(fa->fd > fb->fd)
    return true;
  return false;
}

struct file*
find_file(int fd)
{
  struct file* file = NULL;
  struct file_descriptor *fd_ = NULL;
  struct list_elem* e;
  //binary search can be applied.
  for (e = list_begin (&fd_list); e != list_end (&fd_list);
       e = list_next (e))
    {
      fd_ = list_entry (e, struct file_descriptor, elem);
      if(fd_->fd <= fd)
        break;
    }
    if(fd_!=NULL && fd_->fd == fd)
    {
      file = fd_->file;
    }
  return file;
}


void
syscall_init (void) 
{
  fd_count = 2;
  list_init (&fd_list);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
exit_handle (struct intr_frame *f UNUSED, int status)
{
  thread_current()->ev->exit_value = status;
  thread_exit();
  //need synch.
}

void
exec_handle (struct intr_frame *f, const char *file)
{
  f->eax = (uint32_t)process_execute(file);
  //need synch
}

void
wait_handle (struct intr_frame *f, tid_t pid)
{
  f->eax = process_wait(pid);
}

void
create_handle (struct intr_frame *f, const char *file, unsigned initial_size)
{
  f->eax = filesys_create (file, initial_size);
}

void
remove_handle (struct intr_frame *f, const char *file)
{
   f->eax = filesys_remove (file);
}

void
open_handle (struct intr_frame *f, const char *file)
{
  struct file_descriptor* new_fd = (struct file_descriptor*) malloc(sizeof(struct file_descriptor));
  new_fd->file = filesys_open (file);
  new_fd->fd = fd_count;
  fd_count++;
  list_push_back(&fd_list, &new_fd->elem);
  f->eax = new_fd->fd;
}

void
filesize_handle (struct intr_frame *f, int fd) 
{
    struct file* file = find_file(fd);
    if(file!=NULL)
      {//is buffer valid
      f->eax = inode_length (file_get_inode(file));
      }
}

void
read_handle (struct intr_frame *f, int fd, void *buffer, unsigned size)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      f->eax = file_read (file, buffer, size);
      return;
    }
  else if(fd==0)
  {
    f->eax = input_getc();
    return;
  }
  f->eax = -1;
}

void
write_handle (struct intr_frame *f, int fd, const void *buffer, unsigned size)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
    f->eax = file_write (file, buffer, size);
    return;
    }
  else if(fd==1)
  {
    putbuf(buffer, size);
    f->eax = 1; //is it right?
    return;
  }
  f->eax = 0;
}

void
seek_handle (struct intr_frame *f UNUSED, int fd, unsigned position) 
{  
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is position valid
      file_seek (file, position);
    }
}

void
tell_handle (struct intr_frame *f, int fd) 
{  
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      f->eax = file_tell (file);
      return;
    }
  f->eax = 0;
}

void
close_handle (struct intr_frame *f, int fd)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      file_close (file);
      f->eax = 1;
      return;
    }
  f->eax = 0;
}




static void
syscall_handler (struct intr_frame *f) 
{
  int c;
  int arg0;
  int arg1;
  int arg2;
  c = *((int*)(f->esp));
  switch(c){
    //zero argument
    case SYS_HALT: shutdown_power_off(); break;
    //first argument
    case SYS_EXIT: 
    case SYS_EXEC: 
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE: 
    arg0 = *((int*)(f->esp)+1);

      switch(c){
        case SYS_EXIT: exit_handle(f, (int)arg0); break;
        case SYS_EXEC: exec_handle(f, (char*)arg0); break;
        case SYS_WAIT: wait_handle(f, (tid_t)arg0); break;
        case SYS_REMOVE: remove_handle(f, (char*)arg0); break;
        case SYS_OPEN: open_handle(f, (char*)arg0); break;
        case SYS_FILESIZE: filesize_handle(f, (int)arg0); break;
        case SYS_TELL: tell_handle(f, (int)arg0); break;
        case SYS_CLOSE: close_handle(f, (int)arg0); break;
      }

    break;
    //second argument
    
    case SYS_CREATE: 
    case SYS_SEEK:
    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
      
      switch(c){
        case SYS_CREATE: create_handle(f, (char*)arg0, (unsigned)arg1); break;
        case SYS_SEEK: seek_handle(f, (int)arg0, (unsigned)arg1); break;
      }
    break;
    
    case SYS_READ: 
    case SYS_WRITE: 
    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
    arg2 = *((int*)(f->esp)+3);
      
      switch(c){
        case SYS_READ: read_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
        case SYS_WRITE: write_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
      }
    break;
  }
  printf ("system call!\n");
}
