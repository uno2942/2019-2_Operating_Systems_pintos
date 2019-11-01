#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static int fd_count;

static struct list fd_list;

struct file_descriptor{
    struct file* file;
    int fd;
    struct list_elem elem;
}

bool
fd_less(const struct list_elem *a, const struct list_elem *b, void *aux){
  struct file_descriptor* fa = list_entry (a, struct file_descriptor, elem);
  struct file_descriptor* fb = list_entry (b, struct file_descriptor, elem);
  if(fa->fd_count > fb->fd_count)
    return true;
  return false;
}

struct file*
find_file(int fd)
{
  struct file* file = NULL;
  struct file_descriptor *fd_;
  //binary search can be applied.
  for (e = list_begin (&fd_list); e != list_end (&fd_list);
       e = list_next (e))
    {
      fd_ = list_entry (e, struct file_descriptor, elem);
      if(fd_->fd <= fd)
        break;
    }
    if(fd_->fd == fd)
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
exit_handle (int status)
{
  thread_exit();
  //need synch.
}

void
exec_handle (const char *file)
{
  f->eax = (uint32_t)process_execute(file);
  //need synch
}

void
wait_handle (pid_t pid)
{
  return syscall1 (SYS_WAIT, pid);
}

void
create (const char *file, unsigned initial_size)
{
  f->eax = filesys_create (file, initial_size);
}

void
remove (const char *file)
{
   f->eax = filesys_remove (file);
}

void
open (const char *file)
{
  struct file_descriptor* new_fd = (struct file_descriptor*) malloc(sizeof(struct file_descriptor));
  new_fd->file = filesys_open (file);
  new_fd->fd = fd_count;
  fd_count++;
  list_push_back(&fd_list, &new_fd->elem, NULL);
  f->eax = new_fd->fd;
}

void
filesize (int fd) 
{
    struct file* file = find_file(fd);
    if(file!=NULL)
      {//is buffer valid
      f->eax = inode_length (file->inode);
      }
}

void
read (int fd, void *buffer, unsigned size)
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
write (int fd, const void *buffer, unsigned size)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      f->eax = file_write (file, *buffer, size);
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
seek (int fd, unsigned position) 
{  
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      f->eax = file_seek (file, position);
      return;
    }
  f->eax = 0;
}

void
tell (int fd) 
{  
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      f->eax = file_tell (file, position);
      return;
    }
  f->eax = 0;
}

void
close (int fd)
{
  syscall1 (SYS_CLOSE, fd);
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      file_close (file, position);
      f->eax = 1;
      return;
    }
  f->eax = 0;
}










static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int c;
  int arg0;
  int arg1;
  case = *(f->esp);
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
    case SYS_SEEK:
    case SYS_TELL:
    case SYS_CLOSE: 
    arg0 = *((f->esp)+1);

      switch(c){
        case SYS_EXIT: exit_handle(arg0); break;
        case SYS_EXEC: exec_handle(arg0); break;
        case SYS_WAIT: wait_handle(arg0); break;
        case SYS_REMOVE: remove_handle(arg0); break;
        case SYS_OPEN: open_handle(arg0); break;
        case SYS_FILESIZE: filesize_handle(arg0); break;
        case SYS_SEEK: seek_handle(arg0); break;
        case SYS_TELL: tell_handle(arg0); break;
        case SYS_CLOSE: close_handle(arg0); break;
      }

    break;
    //second argument
    
    case SYS_CREATE: 
    case SYS_READ: 
    case SYS_WRITE: 
    arg0 = *((f->esp)+1);
    arg1 = *((f->esp)+2);
      
      switch(c){
        case SYS_CREATE: create_handle(arg0, arg1); break;
        case SYS_READ: read_handle(arg0, arg1); break;
        case SYS_WRITE: write_handle(arg0, arg1); break;
      }

    break;
  }
  printf ("system call!\n");
  thread_exit ();
}
