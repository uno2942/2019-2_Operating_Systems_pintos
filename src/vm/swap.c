#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

#define PAGE_RATIO_PER_SWAP 256

static bitmap *swap_map;
static lock swap_lock;

void
swap_init (int n)
{
    lock_init(&swap_lock);
    swap_map = bitmap_create (n * 256);
}

void
put_to_swap (struct frame *f)
{

}

void
load_from_swap (struct spage *spage, struct frame *f)
{
    
}

void
clear_swap_table (int n)
{

}
