#include "vm/page.h"
#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/synch.h"

// static struct lock page_lock;

unsigned spage_hash (const struct hash_elem *p_, void *aux UNUSED);
bool spage_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void free_spage_element (struct hash_elem *h_elem, void *aux UNUSED);
unsigned
spage_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spage *p = hash_entry (p_, struct spage, hash_elem);
  return hash_bytes (&p->page, sizeof p->page);
}

/* Returns true if page a precedes page b. */
bool
spage_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spage *a = hash_entry (a_, struct spage, hash_elem);
  const struct spage *b = hash_entry (b_, struct spage, hash_elem);

  return a->page < b->page;
}

void
supplemental_page_table_init (struct hash* sp_table) //Who init lock?
{
    hash_init (sp_table, spage_hash, spage_less, NULL);
}

struct hash_elem *
supplemental_page_table_lookup (struct hash* sp_table, void *page)
{
  struct spage p;
  struct hash_elem *e;
  ASSERT (page != NULL);
//  ASSERT (lock_held_by_current_thread (&page_lock));
  p.page = page;
  e = hash_find (sp_table, &p.hash_elem);
  return e;
}

void
insert_to_supplemental_page_table (struct hash* sp_table, struct spage* spage)
{
//    lock_acquire (&page_lock);
    ASSERT (supplemental_page_table_lookup (sp_table, spage->page)==NULL);

    hash_insert (sp_table, &spage->hash_elem);
//    lock_release (&page_lock);
}

void delete_from_supplemental_page_table (struct hash* sp_table, void *page)
{
//    lock_acquire (&page_lock);
    
    struct hash_elem *h_elem;
    h_elem = supplemental_page_table_lookup (sp_table, page);

    ASSERT (h_elem!=NULL);

    hash_delete (sp_table, h_elem);

    struct spage *spage_temp = hash_entry (h_elem, struct spage, hash_elem);
//    lock_release (&page_lock);
    free(spage_temp);
}

void free_spage_element (struct hash_elem *h_elem, void *aux UNUSED)
{
    struct spage *spage_temp = hash_entry (h_elem, struct spage, hash_elem);
    free(spage_temp);
}

void clear_supplemental_page_table (struct hash* sp_table)
{
  hash_clear (sp_table, free_spage_element);
}
