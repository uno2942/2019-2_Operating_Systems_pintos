#include "vm/page.h"
#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

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
supplemental_page_table_init (struct hash* sp_table)
{
    hash_init (sp_table, spage_hash, spage_less, NULL);
}

//check whether the page is preserved.
struct hash_elem *
supplemental_page_table_lookup (struct hash* sp_table, void *upage)
{
  struct spage p;
  struct hash_elem *e;
  if (upage == NULL)
    return NULL;
  p.upage = upage;
  e = hash_find (sp_table, &p.hash_elem);
  return e;
}

//preserve the page
void
insert_to_supplemental_page_table (struct hash* sp_table, struct spage* spage)
{
    ASSERT (supplemental_page_table_lookup (sp_table, spage->upage)==NULL);

    hash_insert (sp_table, &spage->hash_elem);
}

//free the page in the view of process.
void delete_from_supplemental_page_table (struct hash* sp_table, void *upage)
{
    struct hash_elem *h_elem;
    h_elem = supplemental_page_table_lookup (sp_table, upage);
    ASSERT (h_elem!=NULL);

    hash_delete (sp_table, h_elem);

    struct spage *spage_temp = hash_entry (h_elem, struct spage, hash_elem);
    free(spage_temp);
}

//clear_supplemental_page_table aux function.
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