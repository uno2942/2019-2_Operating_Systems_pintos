#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

/* check given pointer is vaild user address`s
   if invalid, release all resource & terminate process */

void check_user_addr(const *vaddr)
{
  bool is_vaild_uaddr = true;
  /* check it is null pointer */
  if (vaddr == NULL)
  {
    is_vaild_uaddr = false;
  }
  else
  {
    /* check it is unmmapped virtual pointer 
       function from pagedir.c */
    if(lookup_page(active_pd (),vaddr,false) == NULL)
    {
      is_valid_uaddr = false;
    }
    else
    {
      /* check it is kernel address pointer 
         function from vaddr.h */
      is_vaild_uaddr = is_user_vaddr(vaddr);
    }
  }

  /* handle process when invalid */
  if(!is_vaild_uaddr)
  {
    /* need release all resource. lock & malloc */

    /* terminate process */
    thread_exit ();
  }

}