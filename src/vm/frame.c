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
static struct frame *do_eviction (void *upage, bool writable, enum write_to write_to, int32_t where_to_write, uint32_t write_size);
static void clear_frame (struct frame *frame);
static void free_frame (struct frame *frame);
static struct frame* find_victim (void);
enum write_to convert_read_from_to_write_to (enum read_from read_from)
{
    return (enum write_to) read_from;
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
        //do_eviction
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
            case DATA_F:
            case STACK_F: writable = true; break;
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

static void
clear_frame (struct frame *frame)
{   
    ASSERT (lock_held_by_current_thread (&frame_lock));
    
    struct list *upage_list = &frame->upage_list;
    struct upage_for_frame_table *upage_temp;
    struct list_elem *l_elem;
    if (check_and_set_dirty_for_frame (frame))
    {
        switch (frame->write_to)
        {
            case CODE_F: ASSERT (0); break;
            case MMAP_F: //file_write; break;







            break;
            case DATA_F:
            case STACK_F: //write_to_swap; break;
            break;
        }
    }
    clear_target_pte (frame);

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
    
    clear_frame (frame);
    hash_delete(&frame_table, &frame->hash_elem);
    palloc_free_page (frame->kpage);
    free(frame);
}

//maybe not deleted if it is evicted.
bool
delete_upage_from_frame_table (void *kpage, void *upage, struct thread* owner)
{
    struct frame *temp_frame;
    struct upage_for_frame_table *upage_temp;
    struct list *upage_list;
    struct list_elem *l_elem;
    lock_acquire(&frame_lock);

    temp_frame = frame_table_lookup (kpage);
    if (temp_frame == NULL)
        {
            lock_release(&frame_lock);
            return false;
        }

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
        {
            lock_release(&frame_lock);
            return false;
        }
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

bool
delete_frame_from_frame_table (void *kpage)
{
    struct frame *temp_frame;
    lock_acquire(&frame_lock);
    
    temp_frame = frame_table_lookup (kpage);
    
    if (!temp_frame)
    {
        lock_release(&frame_lock);
        return false;
    }
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
do_eviction (void *upage, bool writable,
            enum write_to write_to,
            int32_t where_to_write,
            uint32_t write_size
            )
{
    struct frame *target_f = NULL;
    void *kpage;
    lock_acquire(&frame_lock);

    target_f = find_victim ();
    
    if (target_f == NULL) 
    {
        lock_release(&frame_lock);
        return NULL;
    }

    clear_frame (target_f);
    
    kpage = target_f->kpage;
    hash_delete(&frame_table, &target_f->hash_elem);
    free (target_f);

    #ifndef NDEBUG
    memset (kpage, 0xcc, PGSIZE);
    #endif

    if (!install_page (thread_current(), upage, kpage, writable)) 
    {
        palloc_free_page (kpage);
        lock_release(&frame_lock);
        return NULL;
    }
    
    target_f = make_frame (write_to, where_to_write, write_size,
                           kpage, upage, false);
    if(target_f == NULL)
    {
        PANIC ("malloc fail in eviction");
    }
    hash_insert (&frame_table, &target_f->hash_elem);

    lock_release(&frame_lock);
    return target_f;
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
