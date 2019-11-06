#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <console.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static int fd_count;

//the list having file_descriptor data.
static struct list fd_list;

static struct lock deny_lock;
//file_descriptor to handle abstraction of file for user.

struct file_descriptor{
    struct file* file;        //real file in system.
    int fd;                   //fd value for user.
    tid_t owner;              //the owner of the file.
    struct list_elem elem;    //for list
};

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
void set_deny_of_file_and_close(struct file* file_, tid_t t);

void 
deny_lock_acquire(void)
{
  lock_acquire(&deny_lock);
}

void 
deny_lock_release(void)
{
  lock_release(&deny_lock);
}

//find file having fd value FD.
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

//close files that the owner haves.
void
close_files(tid_t owner)
{
  struct file_descriptor *fd_ = NULL;
  struct list_elem* e;
  //binary search can be applied.
  for (e = list_begin (&fd_list); e != list_end (&fd_list);
       e = list_next (e))
    {
      fd_ = list_entry (e, struct file_descriptor, elem);
      if(fd_->owner == owner)
        {
          e=list_remove(e);
          e=e->prev;
          file_close(fd_->file);
          free(fd_);
        }
    }
}


void
syscall_init (void) 
{
  fd_count = 2;
  list_init (&fd_list);
  lock_init (&deny_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//Before exit, set the exit value of current thread by status and close
//all the files the thread haves.
//Finally, call exit.
//The print of status value and synchronization task for wait() call
//is done in thread_exit.
void
exit_handle (struct intr_frame *f UNUSED, int status)
{
  thread_current()->ev->exit_value = status;
  
  file_close(thread_current()->file);

  thread_exit();
}

/*
  First check the file data, and call process_excute.
  The called process may exit before returning to this handler.
*/
void
exec_handle (struct intr_frame *f, const char *file)
{
  if(file==NULL)
    goto fail;
  check_user_addr(file);
  f->eax = (uint32_t)process_execute(file);
  return;
  fail:
    f->eax = -1;
    return;
  //need synch
}
/*
  If the target does not exit, wait until it exit.
  If it exit, return the exit value.
  Note that only one process can wait one process since only parent can wait.
*/
void
wait_handle (struct intr_frame *f, tid_t pid)
{
  f->eax = process_wait(pid);
}

void
create_handle (struct intr_frame *f, const char *file, unsigned initial_size)
{
  check_user_addr(file);
  f->eax = filesys_create (file, initial_size);
}

void
remove_handle (struct intr_frame *f, const char *file)
{
  check_user_addr(file);
   f->eax = filesys_remove (file);
}

void
open_handle (struct intr_frame *f, const char *file)
{
  if(file==NULL)
    goto fail;
  check_user_addr(file);
  struct file* file_ = filesys_open (file);
  if(file_==NULL)
    goto fail;
  struct file_descriptor* new_fd = (struct file_descriptor*) malloc(sizeof(struct file_descriptor));
  //I need to free it.
  new_fd->fd = fd_count;
  new_fd->file = file_;
  new_fd->owner = thread_current()->tid;
  fd_count++;
  list_push_back(&fd_list, &new_fd->elem);
  f->eax = new_fd->fd;
  return;
  
  fail:
    f->eax = -1;
    return;
}

void
filesize_handle (struct intr_frame *f, int fd) 
{
    struct file* file = find_file(fd);
    if(file!=NULL)
      {//is buffer valid
      f->eax = file_length (file);
      }
}

void
read_handle (struct intr_frame *f, int fd, void *buffer, unsigned size)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      check_user_addr(buffer);
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
  deny_lock_acquire();
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      check_user_addr(buffer);
      f->eax = file_write (file, buffer, size);
      deny_lock_release();
      return;
    }
  else if(fd==1)
  {
    putbuf(buffer, size);
    f->eax = 1; //is it right?
    deny_lock_release();
    return;
  }
  f->eax = 0;
  deny_lock_release();
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
  struct file_descriptor *fd_ = NULL;
  struct list_elem* e;
  tid_t owner = thread_current()->tid;
  //binary search can be applied.

  //find the corresponding file and close it. Set f->eax = 1 and return;
  for (e = list_begin (&fd_list); e != list_end (&fd_list);
       e = list_next (e))
    {
      fd_ = list_entry (e, struct file_descriptor, elem);
      if(fd_->fd == fd && fd_->owner == owner)
        {
          e=list_remove(e);
          e=e->prev;
          file_close(fd_->file);
          free(fd_);
          f->eax = 1;
          return;
        }
    }
  
  //could not find the corresponding file.
  f->eax = 0;
}




//According to the value *((int*)(f->esp)), categorize the interrupt and call
//appropriate handler.
static void
syscall_handler (struct intr_frame *f) 
{
  int c;
  int arg0;
  int arg1;
  int arg2; 
  check_user_addr(f->esp);
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

    check_user_addr((int*)(f->esp)+1);

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

    check_user_addr((int*)(f->esp)+1);
    check_user_addr((int*)(f->esp)+2);

    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
      
      switch(c){
        case SYS_CREATE: create_handle(f, (char*)arg0, (unsigned)arg1); break;
        case SYS_SEEK: seek_handle(f, (int)arg0, (unsigned)arg1); break;
      }
    break;
    
    case SYS_READ: 
    case SYS_WRITE: 

    check_user_addr((int*)(f->esp)+1);
    check_user_addr((int*)(f->esp)+2);
    check_user_addr((int*)(f->esp)+3);

    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
    arg2 = *((int*)(f->esp)+3);
      
      switch(c){
        case SYS_READ: read_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
        case SYS_WRITE: write_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
      }
    break;
  }
//  printf ("system call!\n");
}


/* check given pointer is vaild user address`s
   if invalid, release all resource & terminate process */
void check_user_addr(const void *vaddr)
{
  bool is_vaild_uaddr = true;
  //printf("PHYS_BASE is : %x\n", (int)PHYS_BASE);
  //printf("%x,\t %x\n",(int)vaddr, (int)(vaddr + 3));
  /* check it is null pointer */
  if (vaddr == NULL)
  {
    is_vaild_uaddr = false;
  }
  else
  {
    /* check it is unmmapped virtual pointer 
       function from pagedir.c */
    if(!is_user_vaddr(vaddr) || !is_user_vaddr((char*)vaddr + 3))
    {
      is_vaild_uaddr = false;
    }
    else
    {
      /* check it is kernel address pointer 
         function from vaddr.h */
      is_vaild_uaddr = !( pagedir_get_page (active_pd (),(char*)vaddr) == NULL
    || pagedir_get_page (active_pd (),(char*)vaddr + 3) == NULL ) ;
    }
  }

  /* handle process when invalid */
  if(!is_vaild_uaddr)
  {
    /* need release all resource. lock & malloc 
       close_files is in thread_handle*/

    /* terminate process */
    //thread_current()->ev->exit_value = -1;
    thread_exit ();
  }

}