#include "vm/page.h"
#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

// static struct lock page_lock;

unsigned spage_hash (const struct hash_elem *p_, void *aux UNUSED);
bool spage_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
void free_spage_element (struct hash_elem *h_elem, void *aux UNUSED);
unsigned
spage_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct spage *p = hash_entry (p_, struct spage, hash_elem);
  return hash_bytes (&p->upage, sizeof p->upage);
}

/* Returns true if page a precedes page b. */
bool
spage_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct spage *a = hash_entry (a_, struct spage, hash_elem);
  const struct spage *b = hash_entry (b_, struct spage, hash_elem);

  return a->upage < b->upage;
}

void
supplemental_page_table_init (struct hash* sp_table) //Who init lock?
{
    hash_init (sp_table, spage_hash, spage_less, NULL);
}

struct hash_elem *
supplemental_page_table_lookup (struct hash* sp_table, void *upage)
{
  struct spage p;
  struct hash_elem *e;
  ASSERT (upage != NULL);
//  ASSERT (lock_held_by_current_thread (&page_lock));
  p.upage = upage;
  e = hash_find (sp_table, &p.hash_elem);
  return e;
}

void
insert_to_supplemental_page_table (struct hash* sp_table, struct spage* spage)
{
//    lock_acquire (&page_lock);
    ASSERT (supplemental_page_table_lookup (sp_table, spage->upage)==NULL);

    hash_insert (sp_table, &spage->hash_elem);
//    lock_release (&page_lock);
}

void delete_from_supplemental_page_table (struct hash* sp_table, void *upage)
{
//    lock_acquire (&page_lock);
    
    struct hash_elem *h_elem;
    h_elem = supplemental_page_table_lookup (sp_table, upage);

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

//clear spage_table before exit the process.
void clear_supplemental_page_table (struct hash* sp_table)
{
  hash_clear (sp_table, free_spage_element);
}

/*
void 
clear_supplemental_page_table_mmap (struct hash *sp_table, uint8_t *from, uint8_t *to)
{
  ASSERT ( pg_ofs (from) == 0 && pg_ofs (to - from) % PGSIZE == 0 && to >= from)
  struct thread *cur = thread_current ();
  while (from <= to)
  {
    struct spage *spage = hash_entry (supplemental_page_table_lookup (sp_table, from),
                               struct spage, hash_elem);
    hash_delete (sp_table, &spage->hash_elem);
    free (spage);
    from = from + PGSIZE;
  }
}*/