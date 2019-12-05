#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

#define PAGE_RATIO_PER_SWAP 256

//Like palloc, maintain free swap slots using bitmap.
static struct bitmap *swap_map;
static struct block *block;
static struct lock swap_lock;

static void set_pointer_to_spage (struct frame *f, int swap_idx);
void
swap_init ()
{
    block = block_get_role (BLOCK_SWAP);
    lock_init(&swap_lock);
    swap_map = bitmap_create (block_size (block) * 256);
}

//After modifing swap block, spage should know where to load it in the case of installing page
//before and after eviction.
//If swap_idx < 0, then it is not in swap slot.
static void
set_pointer_to_spage (struct frame *f, int swap_idx)
{
    struct list *upage_list = &f->upage_list;
    struct list_elem *e;
    struct upage_for_frame_table *upage_for_frame;
    struct hash_elem *h_elem;
    struct spage *spage_temp;
    //for each spage, set the read_file == NULL and where_to_read = swap_idx;
    for (e = list_begin (upage_list); e != list_end (upage_list); e = list_next (e))
    {
        //may need spage lock
        upage_for_frame = list_entry (e, struct upage_for_frame_table, list_elem);
        h_elem = supplemental_page_table_lookup (&upage_for_frame->owner->sp_table, 
                                                     upage_for_frame->upage);
        ASSERT (h_elem != NULL)
        spage_temp = hash_entry (h_elem, struct spage, hash_elem);
        spage_temp->read_file = NULL;
        spage_temp->where_to_read = swap_idx;
        //if it was DATA, then it should be transferred to DATA_MOD_P since the data was modified and
        //endtered to this function. (Note that in frame.c, check dirty bit and in the case of dirty)
        //it calls put_to_swap.)
        if (spage_temp->read_from == DATA_P)
          spage_temp->read_from = DATA_MOD_P;
    }
}

//put the data of frame into swap.
size_t
put_to_swap (struct frame *f)
{
  uint32_t sector;
  size_t swap_idx;
  uint8_t *kpage;
  size_t i;
  ASSERT (check_frame_lock ());
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

//load data from swap and put to f->kpage.
void
load_from_swap (struct spage *spage, struct frame *f)
{
  uint32_t sector;
  size_t swap_idx = (uint32_t) spage->where_to_read;
  uint8_t *kpage;
  size_t i;
  ASSERT (f->pin == true); //prevent eviction.
  ASSERT (spage->where_to_read >= 0);
  sector = 8 * swap_idx;
  kpage = f->kpage;
  for (i = 0; i < 8; i++)
  {
      block_read (block, sector + i, kpage + BLOCK_SECTOR_SIZE * i);
  }
  ASSERT (list_size (&f->upage_list) == 1);
  set_pointer_to_spage (f, -1); //I should assume that only one process
                                //accesses to swap table.
  ASSERT (bitmap_all (swap_map, swap_idx, 1));
  bitmap_set_multiple (swap_map, swap_idx, 1, false);
}

//Clear swap slot by deleting nth swap slot from bitmap.
void
clear_swap_slot (int n)
{
  if (n<0)
    return;
  ASSERT (check_frame_lock ());
  ASSERT (bitmap_all (swap_map, n, 1));
  bitmap_set_multiple (swap_map, n, 1, false);
}
