#include "vm/page.h"
#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/synch.h"

static struct lock page_lock;

void
supplemental_page_table_init (void)
{
    lock_init(&page_lock);
}

struct hash_elem *
supplemental_page_table_lookup (struct hash* sp_table,const void *page)
{
  struct spage p;
  struct hash_elem *e;
  ASSERT (page != NULL);
  ASSERT (lock_held_by_current_thread (&page_lock));
  p.page = page;
  e = hash_find (sp_table, &p.hash_elem);
  return e;
}

void
insert_to_supplemental_page_table (struct hash* sp_table, struct spage* spage)                                )
{
    lock_acquire (&page_lock);
    ASSERT (supplemental_page_table_lookup (spage->page)==NULL)

    hash_insert (sp_table, &spage->hash_elem);
    lock_release (&page_lock);
}

void
delete_from_supplemental_page_table (struct hash* sp_table, void *page)
{
    lock_acquire (&page_lock);
    
    struct hash_elem *h_elem;
    h_elem = supplemental_page_table_lookup (sp_table, page);

    ASSERT (h_elem!=NULL);

    hash_delete (sp_table, hash_elem);

    page_temp = hash_entry (h_elem, struct spage, hash_elem);
    lock_release (&page_lock);
    free(page_temp);
}