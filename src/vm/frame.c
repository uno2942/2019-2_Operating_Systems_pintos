#include "vm/frame.h"
#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct hash frame_table;
static struct lock frame_lock;

unsigned frame_hash (const struct hash_elem *p_, void *aux UNUSED);
bool frame_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);

unsigned
frame_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct frame *p = hash_entry (p_, struct frame, hash_elem);
  return hash_bytes (&p->paddr, sizeof p->paddr);
}

/* Returns true if page a precedes page b. */
bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct frame *a = hash_entry (a_, struct frame, hash_elem);
  const struct frame *b = hash_entry (b_, struct frame, hash_elem);

  return a->paddr < b->paddr;
}

void
frame_table_init (void)
{
    hash_init(&frame_table, frame_hash, frame_less, NULL);
    lock_init(&frame_lock);
}

struct frame *
frame_table_lookup (void *paddr)
{
  struct frame f;
  struct hash_elem *e;

  f.paddr = paddr;
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
            void *paddr,
            void *page,
            bool pin
           )
{
    struct frame *temp_frame = (struct frame *) malloc (sizeof (struct frame) );
    
    temp_frame->write_to = write_to;
    temp_frame->where_to_write = where_to_write;
    temp_frame->write_size = write_size;
    temp_frame->paddr = paddr;
    temp_frame->pin = pin;
    
    list_init (&temp_frame->page_list);
    struct page_for_frame_table *page_temp = (struct page_for_frame_table *) malloc (sizeof (struct page_for_frame_table));
    page_temp->page = page;
    page_temp->owner = thread_current ();
    list_push_back (&temp_frame->page_list, &page_temp->list_elem);

    return temp_frame;
}
void
insert_to_frame_table (struct frame *frame)
{
    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    lock_acquire(&frame_lock);
    temp_frame = frame_table_lookup (frame->paddr);
    if (temp_frame != NULL)
        {
            page_temp = (struct page_for_frame_table *) malloc (sizeof (struct page_for_frame_table));
            ASSERT (list_size (&frame->page_list) == 1);

            page_temp->page = list_entry (list_begin (&frame->page_list), 
                                struct page_for_frame_table, list_elem)->page;
            page_temp->owner = thread_current ();
            list_push_back (&temp_frame->page_list, &page_temp->list_elem);
            free (frame);
        }
    else
    {
        hash_insert (&frame_table, &frame->hash_elem);
    }
    
    lock_release(&frame_lock);
    return;
}


bool
delete_page_from_frame_table(void *paddr, void *page, struct thread* owner)
{
    struct list_elem *l_elem;

    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    struct list *page_list;

    lock_acquire(&frame_lock);
    temp_frame = frame_table_lookup (paddr);
    if (!temp_frame)
        {
            lock_release(&frame_lock);
            return false;
        }

    page_list = &temp_frame->page_list;

    if (list_size (page_list) == 1)
    {
        hash_delete(&frame_table, &temp_frame->hash_elem);
        l_elem = list_pop_front (page_list);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
        free(temp_frame);
    }
    else
    {
        l_elem = find_elem_in_page_list(page_list, page, owner);
        if(!l_elem)
            return false;
        list_remove (l_elem);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
    }
    lock_release(&frame_lock);
    return true;
}

bool
delete_frame_from_frame_table (void *paddr)
{
    struct list_elem *l_elem;
    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    struct list *page_list;
    lock_acquire(&frame_lock);
    temp_frame = frame_table_lookup (paddr);
    if (!temp_frame)
        {
            lock_release(&frame_lock);
            return false;
        }
    page_list = &temp_frame->page_list;
    while (list_size (page_list)==0)
    {
        l_elem = list_pop_front (page_list);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
    }
    hash_delete (&frame_table, &temp_frame->hash_elem);
    free (temp_frame);

    lock_release(&frame_lock);
    return true;
}

struct list_elem *
find_elem_in_page_list (struct list *page_list, void *page, struct thread* owner)
{
    struct page_for_frame_table *page_temp;
    struct list_elem *e;
    ASSERT (lock_held_by_current_thread (&frame_lock));

  for (e = list_begin (page_list); e != list_end (page_list);
      e = list_next (e))
      {
          page_temp = list_entry (e, struct page_for_frame_table, list_elem);
          if(page_temp->page == page && page_temp->owner == owner) //is it correct?
          {
              return e;
          }
      }
      return NULL;
}

enum write_to convert_read_from_to_write_to (enum read_from read_from)
{
    return (enum write_to) read_from;
}

struct frame*
do_eviction (void *upage, bool writable,
            enum write_to write_to,
            int32_t where_to_write,
            uint32_t write_size
            )
{
    static ; //for clock algorithm
    static struct hash_iterator iter;
    struct frame *target_f;
    lock_acquire(&frame_lock);
    while (hash_next (&iter))
    {
        target_f = hash_entry (hash_cur (&iter), struct frame, hash_elem);
        if(!check_and_set_accesse_for_frame (target_f))
            break;
    }
    if(hash_cur (&iter) == NULL)
    {
        //maybe there are element in front of clock...
        lock_release(&frame_lock);
        return NULL;
    }
    if(check_and_set_dirty_for_frame (target_f))
    {
        //do dirty process.
    }
    arrange_target_pte (target_f);
    
    #ifndef NDEBUG
    memset (target_f->paddr, 0xcc, PGSIZE);
    #endif
    
    if (!install_page (upage, target_f->paddr, writable)) 
    {
        palloc_free_page (target_f->paddr);
        lock_release(&frame_lock);
        return NULL;
    }
    
    struct list *page_list = &target_f->page_list;
    while (list_size (page_list)==0)
    {
        l_elem = list_pop_front (page_list);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
    }

    hash_delete (&frame_table, &target_f->hash_elem);
    free (target_f);

    hash_insert (&frame_table, &(make_frame (write_to, where_to_write, write_size,
                                           target_f->paddr, upage, false)->hash_elem));
    lock_release(&frame_lock);
    return target_f;
}

bool
check_and_set_accesse_for_frame (struct frame *f)
{
    struct list_elem *e;
    struct list *page_list = &f->page_list;
    struct page_for_frame_table *page_for_frame;
    bool ret = false;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (page_list); e != list_end (page_list);
        e = list_next (e))
    {
        page_for_frame = list_entry (e, struct page_for_frame_table, list_elem);
        ret = pagedir_is_accessed (page_for_frame->owner->pagedir, 
                                   page_for_frame->page) || ret; //check user vaddr
        pagedir_set_accessed (page_for_frame->owner->pagedir, 
                              page_for_frame->page, false);
        ASSERT (pagedir_get_page (page_for_frame->page) == f->paddr);
        ret = pagedir_is_accessed (page_for_frame->owner->pagedir, 
                                   pagedir_get_page (page_for_frame->page)) || ret; //check kernel vaddr
        pagedir_set_accessed (page_for_frame->owner->pagedir, 
                              pagedir_get_page (page_for_frame->page), false);
    }
    return ret;
}

bool
check_and_set_dirty_for_frame (struct frame *f)
{
    struct list_elem *e;
    struct list *page_list = &f->page_list;
    struct page_for_frame_table *page_for_frame;
    bool ret = false;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (page_list); e != list_end (page_list);
        e = list_next (e))
    {
        page_for_frame = list_entry (e, struct page_for_frame_table, list_elem);
        ret = pagedir_is_dirty (page_for_frame->owner->pagedir, 
                                   page_for_frame->page) || ret; //check user vaddr
        pagedir_set_dirty (page_for_frame->owner->pagedir, 
                           page_for_frame->page, false);
        ASSERT (pagedir_get_page (page_for_frame->page) == f->paddr);
        ret = pagedir_is_dirty (page_for_frame->owner->pagedir, 
                                   pagedir_get_page (page_for_frame->page)) || ret; //check kernel vaddr
        pagedir_set_dirty (page_for_frame->owner->pagedir, 
                           pagedir_get_page (page_for_frame->page), false);
    }
    return ret;
}

void
arrange_target_pte (struct frame *f)
{
    struct list_elem* e;
    struct list *page_list = &f->page_list;
    struct page_for_frame_table *page_for_frame;
    ASSERT (lock_held_by_current_thread (&frame_lock));

    //binary search can be applied.

    //find the corresponding file and close it. Set f->eax = 1 and return;
    for (e = list_begin (&page_list); e != list_end (&page_list);
        e = list_next (e))
    {
        page_for_frame = list_entry (e, struct page_for_frame_table, list_elem);
        pagedir_clear_page (page_for_frame->owner->pagedir, 
                            page_for_frame->page); //check user vaddr
    }
    return ret;
}