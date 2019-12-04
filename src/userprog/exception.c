#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <hash.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#define STACK_GROW_LIMIT ( 0xc0000000 - 0x800000 ) /* PHYS_BASE - 8MB */
/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static bool load_page_in_memory (struct file *file, off_t ofs, uint8_t *upage, uint32_t page_read_bytes, uint32_t page_zero_bytes, enum read_from read_from);

static bool stack_page_install(void *fault_addr);
/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  int distance_from_stack_top;


  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();
  
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  
  if( (not_present && user == true) || (not_present && !user && thread_current ()->allow_kernel_panic))
   {
      struct hash* sp_table = &thread_current()->sp_table;
      struct hash_elem *h_elem = supplemental_page_table_lookup (sp_table, pg_round_down (fault_addr));
//      printf("fault: %p\n",pg_round_down (fault_addr));
      if (h_elem == NULL) //maybe STACK fault
      {
//        printf("now address want to access : %x , stack pointer when interrupt occur : %x\n", (unsigned int)fault_addr, (unsigned int)(f->esp));
        
        distance_from_stack_top = (int)(f->esp) - (int)fault_addr;
        if((unsigned int)fault_addr < STACK_GROW_LIMIT)
        {
          //touch heap range : segfault
          goto real_fault;
        }
/*         else if(distance_from_stack_top <= 0)
        {
          //normal stack range : which is replaced - need to call back from swap table
        } */
        else if(distance_from_stack_top > 32)
        {
          //need to handle case : not move esp itself. just access under the esp
//          printf("bad access, not push\n");
          goto real_fault;
        }
        else
        {
          //for case only moving exact esp value. range is stack_grow_limit < fault_addr < stack top case, we do stack growth.
//          printf("page fault but we can stack growth\n");

          //success -> return true  
          if(stack_page_install(fault_addr) == false)
            goto real_fault;
          else
            return;
        }
      }
      struct spage* spage = hash_entry (h_elem, struct spage, hash_elem);
      if(not_present)
      {
         if (load_page_in_memory(spage->read_file, spage->where_to_read, pg_round_down (fault_addr),
                  spage->read_size, PGSIZE - spage->read_size, spage->read_from)
                  == false)
               goto real_fault;
            else
               return;
      }
      else
      {
         goto real_fault;
      }
   }
   else
      goto real_fault;
  
  /* Count page faults. */
real_fault:
  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}

//For now, I assumed that there is no already existing frame in frame table.

//In case of MMAP, CODE, and DATA for first time, read file and put it on kpage
static bool
load_page_in_memory (struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t page_read_bytes, uint32_t page_zero_bytes,
                        enum read_from read_from)
{
   
  ASSERT ((page_read_bytes + page_zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
//  ASSERT (ofs % PGSIZE == 0);

  uint8_t *kpage;
  struct spage *spage;
  struct hash_elem *h_elem;
  struct hash *sp_table = &thread_current ()->sp_table;
  /* Get a page of memory. */
  bool success;
  bool f_lock_held = false;
  
  //I need to consider the case: while doing eviction and putting on swap table
  //the victim page tries to load by this function.
  struct frame *frame = make_frame (convert_read_from_to_write_to (read_from),
                          file, ofs, page_read_bytes, NULL, upage, true);
  if(frame == NULL)
    PANIC ("malloc fail");
  success = insert_to_frame_table (PAL_USER, frame);
  if(success == false)
    PANIC ("palloc fail");

  kpage = frame->kpage;
 //  printf("upage: %p, kpage: %p\n", upage, kpage);
  h_elem = supplemental_page_table_lookup (sp_table, upage);
  spage = hash_entry (h_elem, struct spage, hash_elem);

   //in eviction, these can be changed.
   file = spage->read_file;
   ofs = spage->where_to_read;
   read_from = spage->read_from;

   frame->write_file = file;
   frame->where_to_write = ofs;
   frame->write_to = convert_read_from_to_write_to (read_from);

  ASSERT (spage!=NULL && spage->read_size == page_read_bytes); 
  if (read_from == CODE_P || read_from == MMAP_P || read_from == DATA_P)
   {
      /* Load this page. */
      if (is_file_lock_held)
         f_lock_held = true;
      if (!f_lock_held)
         file_lock_acquire ();
      file_seek (file, ofs);
      success = (file_read (file, kpage, page_read_bytes) == (int) page_read_bytes);
      
      if (!is_file_lock_held)
         file_lock_release ();
      if (success == false)
      {
         delete_upage_from_frame_and_swap_table (upage, thread_current ());
         return false;
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);
   }
   else
      load_from_swap (spage, frame);
      
  pagedir_set_accessed (thread_current ()->pagedir, kpage, false);
  pagedir_set_dirty (thread_current ()->pagedir, kpage, false);
  frame->pin = false;//need lock?
  return true;
}

/* add page for stack until it contain fault_addr
   same as userprog/process.c/setup_stack pathway  */
static bool
stack_page_install(void *fault_addr)
{
    bool success = false;
    struct hash* sp_table = &thread_current()->sp_table;

    //frame part
    struct frame* frame = make_frame (SWAP_F, NULL, -1, PGSIZE, NULL, pg_round_down (fault_addr), false);

    if( frame == NULL)
    {
      return false;
    }

    success = insert_to_frame_table (PAL_USER | PAL_ZERO, frame);
  
    if (success == false)
    {
      free (frame);
      return false;
    }

    //spage part
    struct spage *spage_temp = (struct spage *)malloc (sizeof (struct spage));
    
    if (spage_temp == NULL)
    {
      //roll-back
      delete_upage_from_frame_and_swap_table (pg_round_down (fault_addr), thread_current());
      return false;
    }
    spage_temp->read_from = STACK_P;
    spage_temp->read_file = NULL;
    spage_temp->where_to_read = -1;
    spage_temp->read_size = 0;
    spage_temp->upage = pg_round_down (fault_addr);
    
    insert_to_supplemental_page_table (sp_table, spage_temp);

//    printf("** additional stack allocated position : %x\n", pg_round_down (fault_addr));

  return success;
}