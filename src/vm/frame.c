#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/frame.h"

enum write_to 
{
    CODE, MMAP, DATA, SWAP
};

struct frame
{
    enum write_to write_to;
    void *where_to_write;
    uint32_t *write_size;
    void *paddr;
    bool pin;
    struct list page_list;
    struct hash_elem hash_elem;
};

struct page_for_frame_table
{
    void *page;
    struct thread *owner;
    struct list_elem list_elem;
};

static struct hash frame_table;
static struct lock frame_lock;

void
frame_table_init (void)
{
    hash_init(&frame_table);
    lock_init(&frame_lock);
}

struct hash_elem *
frame_table_lookup (const void *paddr)
{
  struct frame f;
  struct hash_elem *e;

  f.paddr = paddr;
  ASSERT (lock_held_by_current_thread (&frame_lock));
  e = hash_find (&frame_table, &f.hash_elem);
  return e;
}

void
insert_to_hash_table (enum write_to write_to,
                           void *where_to_write,
                           uint32_t *write_size,
                           void *paddr,
                           void *page;
                           bool pin
                     )
{
    struct hash_elem *e
    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    lock_acquire(&frame_lock);
    e = frame_table_lookup (paddr);
    if (!e)
        {
            temp_frame = hash_entry (e, struct frame, hash_elem);
            page_temp = (struct pn *) malloc (sizeof (struct page_for_frame_table));
            page_temp->page = page;
            page_temp->onwer = thread_current ();
            list_push_back (temp_frame->page_list, &page_temp->list_elem);
        }
    else
    {
        temp_frame = (struct frame *) malloc (1, sizeof (struct frame) );
        
        temp_frame->write_to = write_to;
        temp_frame->where_to_write = where_to_write;
        temp_frame->write_size = write_size;
        temp_frame->paddr = paddr;
        temp_frame->pin = pin;
        
        list_init (&temp_frame->page_list);
        page_temp = (struct pn *) malloc (sizeof (struct page_for_frame_table));
        page_temp->page = page;
        page_temp->onwer = thread_current ();
        list_push_back (temp_frame->page_list, &page_temp->list_elem);
        
        hash_insert (&frame_table, &temp_frame->hash_elem);
    }
    lock_release(&frame_lock);
    return;
}


bool
delete_page_from_frame_table(void *paddr, void *page, struct thread* owner)
{
    struct hash_elem *h_elem;
    struct list_elem *l_elem;

    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    struct list *page_list;

    lock_acquire(&frame_lock);
    h_elem = frame_table_lookup (paddr);
    if (!h_elem)
        {
            lock_release(&frame_lock);
            return false;
        }

    temp_frame = hash_entry (h_elem, struct frame, hash_elem);
    page_list = &temp_frame->page_list;

    if (list_size (page_list) == 1)
    {
        hash_delete(&frame_table, h_elem);
        l_elem = list_pop_front (page_list);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
        free(temp_frame);
    }
    else
    {
        l_elem = find_elem_in_page_list(page_list, page,owner);
        list_remove (l_elem);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
    }
    lock_release(&frame_lock);
    return true;
}

bool
delete_frame_from_frame_table(void *paddr, struct thread* owner)
{
    struct hash_elem *h_elem;
    struct list_elem *l_elem;
    struct frame *temp_frame;
    struct page_for_frame_table *page_temp;
    struct list *page_list;
    lock_acquire(&frame_lock);
    h_elem = frame_table_lookup (paddr);
    if (!h_elem)
        {
            lock_release(&frame_lock);
            return false;
        }
    temp_frame = hash_entry (h_elem, struct frame, hash_elem);
    
    page_list = &temp_frame->page_list;
    while (list_size (p_list)==0)
    {
        l_elem = list_pop_front (page_list);
        page_temp = list_entry (l_elem, struct page_for_frame_table, list_elem);
        free(page_temp);
    }
    hash_delete (&frame_table, h_elem);
    free (temp_frame);

    lock_release(&frame_lock);
    return true;
}

struct list_elem*
find_elem_in_page_list (struct list *page_list, void *page, struct thread* owner)
{
    struct page_for_frame_table *page_temp;
    struct list_elem *e;
    ASSERT (lock_held_by_current_thread (&frame_lock));

  for (e = list_begin (&page_list); e != list_end (&page_list);
      e = list_next (e))
      {
          page_temp = list_entry (e, struct page_for_frame_table, list_elem);
          if(page_temp->page == page && page_temp->owner == page_temp)
          {
              return e;
          }
      }
      return NULL;
}