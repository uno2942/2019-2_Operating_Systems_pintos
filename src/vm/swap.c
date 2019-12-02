#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

#define PAGE_RATIO_PER_SWAP 256

static bitmap *swap_map;
struct block *block;
static lock swap_lock;

void
swap_init (int n)
{
    block = block_get_role (BLOCK_SWAP);
    lock_init(&swap_lock);
    swap_map = bitmap_create (n * 256);
}

void
set_pointer_to_spage (struct frame *f, size_t swap_idx)
{
    struct list *upage_list = &f->upage_list;
    struct list_elem *e;
    struct upage_for_frame_table *upage_for_frame;
    struct spage *spage_temp;
    for (e = list_begin (upage_list); e != list_end (upage_list); e = list_next (e))
    {
        //may need spage lock
        upage_for_frame = list_entry (e, struct upage_for_frame_table, list_elem);
        spage_temp = supplemental_page_table_lookup (upage_for_frame->owner, 
                                                     upage_for_frame->upage);
        ASSERT (spage_temp != NULL)
        spage_temp->where_to_read = swap_idx;
    }
}


size_t
put_to_swap (struct frame *f)
{
  uint32_t sector;
  size_t swap_idx;
  uint8_t *kpage;
  size_t i;
  lock_acquire (&swap_lock);
  swap_idx = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);

  if (swap_idx != BITMAP_ERROR)
    sector = 8 * swap_idx;
  else
    PANIC ("SWAP PANIC");
  kpage = f->kpage;
  for (i = 0; i < 8; i++)
  {
      block_write (block, sector + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  set_pointer_to_spage (f, swap_idx);
  return swap_idx;
}

void
load_from_swap (struct spage *spage)
{
  uint32_t sector;
  size_t swap_idx = (uint32_t) spage->where_to_read;
  uint8_t *kpage;
  size_t i;

  ASSERT (spage->where_to_write >= 0);
  sector = 8 * swap_idx;
  kpage = f->kpage;
  for (i = 0; i < 8; i++)
  {
      block_read (block, sector + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  ASSERT (&list_size (f->upage_list) == 1);
  set_pointer_to_spage (f, -1); //I should assume that only one process
                                //accesses to swap table.
  ASSERT (bitmap_all (swap_map, swap_idx, 1));
  bitmap_set_multiple (swap_map, swap_idx, 1, false);
}

void
clear_swap_table (int n)
{

}
