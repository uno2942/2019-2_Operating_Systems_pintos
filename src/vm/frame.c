#include "vm/frame.h"
#include <hash.h>
#include <list.h>
#include <string.h>
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"
static struct hash frame_table;
static struct lock frame_lock;

unsigned frame_hash (const struct hash_elem *p_, void *aux UNUSED);
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

bool check_and_set_accesse_for_frame (struct frame *f);
bool check_and_set_dirty_for_frame (struct frame *f);
void clear_target_pte (struct frame *f);

static bool install_page (struct thread *t, void *upage, void *kpage, bool writable);
static struct frame *frame_table_lookup (void *kpage);
static struct list_elem *find_elem_in_upage_list (struct list *upage_list, void *upage, struct thread* owner);
static struct frame* do_eviction (struct frame* frame);
static void clear_frame (struct frame *frame, bool is_exit);
static void free_frame (struct frame *frame);
static struct frame* find_victim (void);
enum write_to convert_read_from_to_write_to (enum read_from read_from)
{
    switch (read_from)
    {
        case CODE_P: return CODE_F;
        case MMAP_P: return MMAP_F;
        case DATA_P:
        case STACK_P: return SWAP_F;
        default: ASSERT (0);
    }
}

unsigned
frame_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct frame *p = hash_entry (p_, struct frame, hash_elem);
  return hash_bytes (&p->kpage, sizeof p->kpage);
}

bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct frame *a = hash_entry (a_, struct frame, hash_elem);
  const struct frame *b = hash_entry (b_, struct frame, hash_elem);

  return a->kpage < b->kpage;
}

static struct list_elem *
find_elem_in_upage_list (struct list *upage_list, void *upage, struct thread* owner)
{
    struct upage_for_frame_table *upage_temp;
    struct list_elem *e;
    ASSERT (lock_held_by_current_thread (&frame_lock));

  for (e = list_begin (upage_list); e != list_end (upage_list);
      e = list_next (e))
      {
          upage_temp = list_entry (e, struct upage_for_frame_table, list_elem);
          if(upage_temp->upage == upage && upage_temp->owner == owner) //is it correct?
          {
              return e;
          }
      }
      return NULL;
}

void
frame_table_init (void)
{
    hash_init(&frame_table, frame_hash, frame_less, NULL);
    lock_init(&frame_lock);
}

static struct frame *
frame_table_lookup (void *kpage)
{
  struct frame f;
  struct hash_elem *e;

  f.kpage = kpage;
  ASSERT (lock_held_by_current_thread (&frame_lock));
  e = hash_find (&frame_table, &f.hash_elem);
  if(e == NULL)
    return NULL;
  return hash_entry (e, struct frame, hash_elem);
}

struct frame*
make_frame (enum write_to write_to,
            struct file *write_file,
            int32_t where_to_write,
            uint32_t write_size,
            void *kpage,
            void *upage,
            bool pin
           )
{
    struct frame *temp_frame = (struct frame *) malloc (sizeof (struct frame) );
    
    if (temp_frame == NULL)
        return NULL;
    
    temp_frame->write_to = write_to;
    temp_frame->write_file = write_file;
    temp_frame->where_to_write = where_to_write;
    temp_frame->write_size = write_size;
    temp_frame->kpage = kpage;
    temp_frame->pin = pin;
    
    list_init (&temp_frame->upage_list);
    struct upage_for_frame_table *upage_temp = (struct upage_for_frame_table *) malloc (sizeof (struct upage_for_frame_table));
    
    if(upage_temp == NULL)
        return NULL;

    upage_temp->upage = upage;
    upage_temp->owner = thread_current ();
    list_push_back (&temp_frame->upage_list, &upage_temp->list_elem);

    return temp_frame;
}


//Allocate new kpage, (now, sharing is unable) and put it to FRAMAE->kpage
//does not free FRAMAE.
bool
insert_to_frame_table (enum palloc_flags flags, struct frame *frame)
{
    struct upage_for_frame_table *upage_temp;
    bool success = true;
    struct list_elem* e;
    bool writable;
    lock_acquire(&frame_lock);

    frame->kpage = palloc_get_page (flags);
    if (frame->kpage == NULL)
    {
        if (do_eviction (frame) == NULL)
            PANIC ("Unable to allocate page");
        if (flags & PAL_ZERO)
            memset (frame->kpage, 0, PGSIZE);
    }
    else
        ASSERT (frame_table_lookup (frame->kpage) == NULL);
    //temp_frame = frame_table_lookup (frame->kpage);
    /*
    if (temp_frame != NULL) // maybe for sharing
        {
            upage_temp = (struct upage_for_frame_table *) malloc (sizeof (struct upage_for_frame_table));
            ASSERT (list_size (&frame->upage_list) == 1);

            upage_temp->upage = list_entry (list_begin (&frame->upage_list), 
                                struct upage_for_frame_table, list_elem)->upage;
            upage_temp->owner = thread_current ();
            list_push_back (&temp_frame->upage_list, &upage_temp->list_elem);
            free (frame);
        }
    else
    {*/
    
//    }
    ASSERT (list_size (&frame->upage_list) > 0);
    

    for (e = list_begin (&frame->upage_list); e != list_end (&frame->upage_list);
         e = list_next (e))
    {
        upage_temp = list_entry (e, struct upage_for_frame_table, list_elem);
        switch (frame->write_to)
        {
            case CODE_F: writable = false; break;
            case MMAP_F:
            case SWAP_F: writable = true; break;
            default: ASSERT (0);
        }
        /* Add the page to the process's address space. */
        success = success && install_page (upage_temp->owner, upage_temp->upage, frame->kpage, writable);
        if (success == false)
        {
            palloc_free_page (frame->kpage);
            struct list_elem *e2;
            e = list_next (e);
            for (e2 = list_begin (&frame->upage_list); e2 != e;
                 e2 = list_next (e2))
            {
                upage_temp = list_entry (e2, struct upage_for_frame_table, list_elem);
                pagedir_clear_page (upage_temp->owner->pagedir, upage_temp->upage);
            }
            lock_release(&frame_lock);
            return success;
        }
    }
    
    hash_insert (&frame_table, &frame->hash_elem);

    lock_release(&frame_lock);
    return success;
}


//if the process exit, then don't write to swap for DATA, SWAP.
//Only deal with upage_list and do clear_target_pte.
//Since the frame will be replaced or deleted, save the data.
static void
clear_frame (struct frame *frame, bool is_exit)
{   
    ASSERT (lock_held_by_current_thread (&frame_lock));
    
    struct list *upage_list = &frame->upage_list;
    struct upage_for_frame_table *upage_temp;
    struct list_elem *l_elem;
    
    //first, disconnect the connection to previous process.
    clear_target_pte (frame);
    //if it is dirty, do something.
    if (check_and_set_dirty_for_frame (frame))
    {
        switch (frame->write_to)
        {
            case CODE_F: ASSERT (0); break;
            case MMAP_F: 
                file_lock_acquire ();
                file_seek (frame->write_file, frame->where_to_write);
                file_write (frame->write_file, frame->kpage, frame->write_size);
                file_lock_release ();
                break;
            case SWAP_F:
                if (!is_exit)
                    put_to_swap (frame);
                break;
        }
    }

    while (list_size (upage_list) == 0)
    {
        l_elem = list_pop_front (upage_list);
        upage_temp = list_entry (l_elem, struct upage_for_frame_table, list_elem);
        free(upage_temp);
    }
}

static void
free_frame (struct frame *frame)
{   
    ASSERT (lock_held_by_current_thread (&frame_lock));
    
    clear_frame (frame, true);
    hash_delete(&frame_table, &frame->hash_elem);
    palloc_free_page (frame->kpage);
    free(frame);
}

//maybe not deleted if it is evicted.
bool
delete_upage_from_frame_and_swap_table (void *upage, struct thread* owner)
{
    struct frame *temp_frame;
    struct upage_for_frame_table *upage_temp;
    struct list *upage_list;
    struct list_elem *l_elem;
    void *kpage;
    lock_acquire(&frame_lock);
    
    ASSERT (upage != NULL && owner != NULL);
    
    kpage = pagedir_get_page (owner->pagedir, upage);
    
    if(kpage == NULL)
    {
        struct hash_elem *h_elem = supplemental_page_table_lookup (&owner->sp_table, upage);
        
        ASSERT (h_elem != NULL)
        struct spage *spage_temp = hash_entry (h_elem, struct spage, hash_elem);
        if (convert_read_from_to_write_to (spage_temp->read_from) == SWAP_F)
        {
            clear_swap_slot (spage_temp->where_to_read);
            spage_temp->where_to_read = -1;
        }
        lock_release(&frame_lock);
        return true;
    }
    
    temp_frame = frame_table_lookup (kpage);
    ASSERT (temp_frame != NULL)

    upage_list = &temp_frame->upage_list;

    if (list_size (upage_list) == 1)
    {
        upage_temp = list_entry (list_begin (upage_list), struct upage_for_frame_table,
                                 list_elem);
        if (upage_temp->upage == upage && upage_temp->owner == owner)
        {
            free_frame (temp_frame);
        }
        else
            PANIC ("Not valid deletion");
    }
    else
    {
        l_elem = find_elem_in_upage_list(upage_list, upage, owner);
        if(!l_elem)
            return false;
        list_remove (l_elem);
        upage_temp = list_entry (l_elem, struct upage_for_frame_table, list_elem);
        pagedir_clear_page (upage_temp->owner->pagedir, upage_temp->upage);
        free(upage_temp);
    }
    lock_release(&frame_lock);
    return true;
}

//may be not used.
bool
delete_frame_from_frame_table (void *kpage)
{
    struct frame *temp_frame;
    struct upage_for_frame_table *upage_temp;
    struct list *upage_list;
    struct hash_elem *h_elem;
    struct spage *spage_temp;
    lock_acquire(&frame_lock);
    
    temp_frame = frame_table_lookup (kpage);
    
    if (!temp_frame)
    {
        if (temp_frame->write_to == SWAP_F)
        {
            upage_list = &temp_frame->upage_list;
            upage_temp = list_entry (list_begin (upage_list), struct upage_for_frame_table,
                                     list_elem);
            h_elem = supplemental_page_table_lookup (&upage_temp->owner->sp_table, upage_temp->upage);
            ASSERT (h_elem != NULL)
            spage_temp = hash_entry (h_elem, struct spage, hash_elem);
            clear_swap_slot (spage_temp->where_to_read);
            //need to delete connection from spage to swap
        }
    }
    else
        free_frame (temp_frame);

    lock_release(&frame_lock);
    return true;
}
























static struct frame*
find_victim ()
{
//    static ; //for clock algorithm
    static struct hash_iterator iter;
    struct frame *target_f = NULL;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    while (hash_next (&iter))
    {
        target_f = hash_entry (hash_cur (&iter), struct frame, hash_elem);
        if(!check_and_set_accesse_for_frame (target_f))
            break;
    }
    if(hash_cur (&iter) == NULL)
    {
        //maybe there are element in front of clock...
        return NULL;
    }
    if(target_f == NULL)
    {
        return NULL;
    }
    return target_f;
}



static struct frame*
do_eviction (struct frame* frame)
{
    struct frame *target_f = NULL;
    void *kpage;
    void *upage;
    bool writable = true;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    target_f = find_victim ();
    
    if (target_f == NULL) 
    {
        lock_release(&frame_lock);
        return NULL;
    }

    clear_frame (target_f, false);
    
    kpage = target_f->kpage;
    hash_delete(&frame_table, &target_f->hash_elem);
    free (target_f);


    if (frame->write_to == CODE_F)
        writable = false;
    
    ASSERT (list_size (&frame->upage_list) == 1);
    upage = list_entry (list_begin (&frame->upage_list), 
                        struct upage_for_frame_table, list_elem)->upage; 
    
    if (!install_page (thread_current(), upage, kpage, writable)) 
    {
        palloc_free_page (kpage);
        lock_release(&frame_lock);
        return NULL;
    }
    
    frame->kpage = kpage;
    hash_insert (&frame_table, &frame->hash_elem);

    return frame;
}

bool
check_and_set_accesse_for_frame (struct frame *f)
{
    struct list_elem *e;
    struct list *upage_list = &f->upage_list;
    struct upage_for_frame_table *upage_for_frame;
    bool ret = false;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (upage_list); e != list_end (upage_list);
        e = list_next (e))
    {
        upage_for_frame = list_entry (e, struct upage_for_frame_table, list_elem);
        ret = pagedir_is_accessed (upage_for_frame->owner->pagedir, 
                                   upage_for_frame->upage) || ret; //check user vaddr
        
        pagedir_set_accessed (upage_for_frame->owner->pagedir, 
                              upage_for_frame->upage, false);
        
        void *kpage = pagedir_get_page (upage_for_frame->owner->pagedir, upage_for_frame->upage);
        ASSERT (kpage == f->kpage);
        
        ret = pagedir_is_accessed (upage_for_frame->owner->pagedir, kpage) || ret; //check kernel vaddr

        pagedir_set_accessed (upage_for_frame->owner->pagedir, kpage, false);
    }
    return ret;
}

bool
check_and_set_dirty_for_frame (struct frame *f)
{
    struct list_elem *e;
    struct list *upage_list = &f->upage_list;
    struct upage_for_frame_table *upage_for_frame;
    bool ret = false;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (upage_list); e != list_end (upage_list);
        e = list_next (e))
    {
        upage_for_frame = list_entry (e, struct upage_for_frame_table, list_elem);
        ret = pagedir_is_dirty (upage_for_frame->owner->pagedir, 
                                   upage_for_frame->upage) || ret; //check user vaddr
        pagedir_set_dirty (upage_for_frame->owner->pagedir, 
                           upage_for_frame->upage, false);
        void *kpage = pagedir_get_page (upage_for_frame->owner->pagedir, upage_for_frame->upage);
        
        ASSERT (kpage == f->kpage);

        ret = pagedir_is_dirty (upage_for_frame->owner->pagedir, kpage) || ret; //check kernel vaddr
        pagedir_set_dirty (upage_for_frame->owner->pagedir, kpage, false);
    }
    return ret;
}

void
clear_target_pte (struct frame *f)
{
    struct list_elem* e;
    struct list *upage_list = &f->upage_list;
    struct upage_for_frame_table *upage_for_frame;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (upage_list); e != list_end (upage_list);
        e = list_next (e))
    {
        upage_for_frame = list_entry (e, struct upage_for_frame_table, list_elem);
        pagedir_clear_page (upage_for_frame->owner->pagedir, 
                            upage_for_frame->upage); //check user vaddr
    }
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (struct thread *t, void *upage, void *kpage, bool writable)
{
//   printf("A\n");
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool
check_frame_lock ()
{
    return lock_held_by_current_thread (&frame_lock);
}