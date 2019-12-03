#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <console.h>
#include <round.h>
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
#include "vm/page.h"
#include "vm/frame.h"

static int fd_count;

//the list having file_descriptor data.
static struct list fd_list;

static struct lock file_lock;
//file_descriptor to handle abstraction of file for user.

struct file_descriptor{
    struct file* file;        //real file in system.
    int fd;                   //fd value for user.
    tid_t owner;              //the owner of the file.
    struct list_elem elem;    //for list
};

struct mmap_elem{
  struct file* file;
  void *addr;
  int length;
  int id;
  struct list_elem elem;
};

struct mmap_elem *find_mmap_elem (int id);
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
void mmap_handle (struct intr_frame *f, int fd, void *addr);
void munmap_handle (struct intr_frame *f UNUSED, int id);

static void syscall_handler (struct intr_frame *);

void 
file_lock_acquire(void)
{
  lock_acquire(&file_lock);
}

void 
file_lock_release(void)
{
  lock_release(&file_lock);
}

//find file having fd value FD.
struct file*
find_file(int fd)
{
  struct file* file = NULL;
  struct file_descriptor *fd_ = NULL;
  struct list_elem* e;
  //binary search can be applied.
  lock_acquire(&file_lock);
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
  lock_release(&file_lock);
  return file;
}

//close files that the owner haves.
void
close_files(tid_t owner)
{
  struct file_descriptor *fd_ = NULL;
  struct list_elem* e;
  //binary search can be applied.
  lock_acquire(&file_lock);
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
  lock_release(&file_lock);
}


void
syscall_init (void) 
{
  fd_count = 2;
  list_init (&fd_list);
  lock_init (&file_lock);
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

  thread_exit();
}

/*
  First check the file data, and call process_excute.
  The called process may exit before returning to this handler.
*/
void
exec_handle (struct intr_frame *f, const char *file)
{
  if(!check_user_addr(file))
    exit_handle(NULL, -1);
  f->eax = (uint32_t)process_execute(file);
  return;
  
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
  if(!check_user_addr(file))
    exit_handle(NULL, -1);
  lock_acquire(&file_lock);
  f->eax = filesys_create (file, initial_size);
  lock_release(&file_lock);
  
}

void
remove_handle (struct intr_frame *f, const char *file)
{
  if(!check_user_addr(file))
    exit_handle(NULL, -1);
  lock_acquire(&file_lock);
   f->eax = filesys_remove (file);
  lock_release(&file_lock);
}

void
open_handle (struct intr_frame *f, const char *file)
{
  if(!check_user_addr(file))
    exit_handle(NULL, -1);
  lock_acquire(&file_lock);
  struct file* file_ = filesys_open (file);
  lock_release(&file_lock);
  if(file_==NULL)
    {
      f->eax = -1;
      return;
    }
  lock_acquire(&file_lock);
  struct file_descriptor* new_fd = (struct file_descriptor*) malloc(sizeof(struct file_descriptor));
  //I need to free it.
  new_fd->fd = fd_count;
  new_fd->file = file_;
  new_fd->owner = thread_current()->tid;
  fd_count++;
  list_push_back(&fd_list, &new_fd->elem);
  lock_release(&file_lock);
  f->eax = new_fd->fd;
  return;
}

void
filesize_handle (struct intr_frame *f, int fd) 
{
    struct file* file = find_file(fd);
    if(file!=NULL)
      {
      lock_acquire(&file_lock);
      f->eax = file_length (file);
      lock_release(&file_lock);
      }
}

void
read_handle (struct intr_frame *f, int fd, void *buffer, unsigned size)
{
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      //need to check from buffer to buffer+size
    if(!check_user_addr(buffer))
      {
//        printf("asdf\n");
        exit_handle(NULL, -1);
      }
    file_lock_acquire();
    f->eax = file_read (file, buffer, size);
    file_lock_release();
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
    if(!check_user_addr(buffer))
      exit_handle(NULL, -1);
      file_lock_acquire();
      f->eax = file_write (file, buffer, size);
      file_lock_release();
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
      lock_acquire(&file_lock);
      file_seek (file, position);
      lock_release(&file_lock);
    }
}

void
tell_handle (struct intr_frame *f, int fd) 
{  
  struct file* file = find_file(fd);
  if(file!=NULL)
    {
      //is buffer valid
      lock_acquire(&file_lock);
      f->eax = file_tell (file);
      lock_release(&file_lock);
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
  lock_acquire(&file_lock);
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
          lock_release(&file_lock);
          return;
        }
    }
  lock_release(&file_lock);
  //could not find the corresponding file.
  f->eax = 0;
}


static bool load_mmap (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes);
struct mmap_elem *find_mmap_elem (int id);

struct mmap_elem *
find_mmap_elem (int id)
{
  struct list_elem* e;
  struct list *mmap_list = &thread_current ()->mmap_list;
  struct mmap_elem *mmap_elem;
  //binary search can be applied.
  for (e = list_begin (mmap_list); e != list_end (mmap_list);
       e = list_next (e))
    {
      mmap_elem = list_entry (e, struct mmap_elem, elem);
      if(mmap_elem->id == id)
        return mmap_elem;
    }
  return NULL;
}

void
mmap_handle (struct intr_frame *f, int fd, void *addr)
{
  struct file* file = find_file(fd);
  struct file* mmap_file;
  off_t length;
  lock_acquire(&file_lock);
  mmap_file = file_reopen (file);
  length = file_length (mmap_file);
  lock_release(&file_lock);


  //check address and properties.

  if(load_mmap (mmap_file, 0, addr, length, ROUND_UP (length, PGSIZE)-length))
  {
    struct mmap_elem *mmap_elem = malloc (sizeof (struct mmap_elem));
    mmap_elem->file = mmap_file;
    mmap_elem->addr = addr;
    mmap_elem->length = length;
    mmap_elem->id = ++(thread_current()->mmap_t_allocator);
    list_push_back (&thread_current ()->mmap_list, &mmap_elem->elem);
    f->eax = mmap_elem->id;
  }
  else
    f->eax = -1;
  
}

//Assume upage and other properties are legal.
static bool
load_mmap (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  
  struct hash* sp_table = &thread_current()->sp_table;
  struct hash_elem* h_elem;
  
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      h_elem = supplemental_page_table_lookup (sp_table, upage);
    
      ASSERT (h_elem == NULL);
      struct spage *spage_temp = (struct spage *)malloc (sizeof (struct spage));
      ASSERT (spage_temp != NULL)
      /*if (spage_temp == NULL)
        {
          //return to original state.
          return false; 
        }
      */
      spage_temp->where_to_read = ofs;
      spage_temp->read_file = file;
      spage_temp->read_size = page_read_bytes;
      spage_temp->upage = upage;
      spage_temp->read_from = MMAP_P;
      insert_to_supplemental_page_table (sp_table, spage_temp);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }
  return true;
}


void
munmap_handle (struct intr_frame *f UNUSED, int id)
{
  struct mmap_elem *mmap_elem = find_mmap_elem (id);
  struct thread *cur = thread_current ();
  struct hash* sp_table = &cur->sp_table;
  uint8_t *start_addr = mmap_elem->addr;
  uint8_t *now_addr;
  void *kaddr;
  int i=0;
  while (i < mmap_elem->length)
  {
    now_addr = start_addr + i;
    delete_upage_from_frame_table (now_addr, cur);
    delete_from_supplemental_page_table (sp_table, now_addr);
    i += PGSIZE;
  }
  lock_acquire (&file_lock);
  file_close (mmap_elem->file);
  lock_release (&file_lock);
  list_remove (&mmap_elem->elem);
  free (mmap_elem);
}

void
clear_mmap_list_for_exit ()
{
  struct thread *cur = thread_current ();
  struct mmap_elem *mmap_temp;
  while (list_size (&cur->mmap_list) > 0)
  {
    mmap_temp = list_entry (list_begin (&cur->mmap_list), struct mmap_elem, elem);
    munmap_handle (NULL, mmap_temp->id);
  }
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
  if(!check_user_addr(f->esp))
    exit_handle(NULL, -1);
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
    case SYS_MUNMAP:

    if(!check_user_addr((int*)(f->esp)+1))
      exit_handle(NULL, -1);

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
        case SYS_MUNMAP: munmap_handle (f, (int)arg0); break;
      }

    break;
    //second argument
    
    case SYS_CREATE: 
    case SYS_SEEK:
    case SYS_MMAP:
    if((!check_user_addr((int*)(f->esp)+1)) || (!check_user_addr((int*)(f->esp)+2)))
      exit_handle(NULL, -1);

    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
      
      switch(c){
        case SYS_CREATE: create_handle(f, (char*)arg0, (unsigned)arg1); break;
        case SYS_SEEK: seek_handle(f, (int)arg0, (unsigned)arg1); break;
        case SYS_MMAP: mmap_handle(f, (int)arg0, (void *)arg1); break;
      }
    break;
    
    case SYS_READ: 
    case SYS_WRITE: 

    if((!check_user_addr((int*)(f->esp)+1)) || (!check_user_addr((int*)(f->esp)+2))
        || (!check_user_addr((int*)(f->esp)+3)))
      exit_handle(NULL, -1);

    arg0 = *((int*)(f->esp)+1);
    arg1 = *((int*)(f->esp)+2);
    arg2 = *((int*)(f->esp)+3);
      
      switch(c){
        case SYS_READ: read_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
        case SYS_WRITE: write_handle(f, (int)arg0, (void*)arg1, (unsigned)arg2); break;
      }
    break;
  }
}

