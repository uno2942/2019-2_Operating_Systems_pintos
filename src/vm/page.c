#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"

struct page
{
    enum read_from read_from;
    void *where_to_read;        // If READ_FROM == SWAP, then it indicates slot #.
    uint32_t read_size;
    void *page;
    void *frame;
    struct hash_elem hash_elem;
};

static struct hash supplemental_page_table;
static struct lock page_lock;

void
supplemental_page_table_init (void)
{
    hash_init(&supplemental_page_table);
    lock_init(&page_lock);
}

struct hash_elem *
supplemental_page_table_lookup (const void *page)
{
  struct page p;
  struct hash_elem *e;

  p.page = page;
  ASSERT (lock_held_by_current_thread (&page_lock));
  e = hash_find (&supplemental_page_table, &p.hash_elem);
  return e;
}

void
insert_to_supplemental_page_table(enum read_from read_from,
                                 void *where_to_read,
                                 uint32_t read_size,
                                 void *page,
                                 void *frame,
                                 )
{
    lock_acquire (&page_lock);
    ASSERT (supplemental_page_table_lookup (page)!=NULL)
    
    struct page *page_temp;
    page_temp = malloc (sizeof (struct page));
    
    page_temp->read_from = read_from;
    page_temp->where_to_read = where_to_read;
    page_temp->read_size = read_size;
    page_temp->page = page;
    page_temp->frame = frame;

    hash_insert (&supplemental_page_table, &page_temp->hash_elem);
    lock_release (&page_lock);
}

void
delete_from_supplemental_page_table(void *page)
{
    lock_acquire (&page_lock);
    
    struct hash_elem *h_elem;
    struct page *page_temp;
    h_elem = supplemental_page_table_lookup (page);
    
    ASSERT (h_elem!=NULL);

    hash_delete (&supplemental_page_table, hash_elem);

    page_temp = hash_entry (h_elem, struct page, hash_elem);
    free(page_temp);
}