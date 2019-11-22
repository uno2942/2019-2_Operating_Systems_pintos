#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"

static bitmap *swap_map;
static lock swap_lock;

void swap_init (uint n)
{
    lock_init(&swap_lock);
    swap_map = bitmap_create (n);
}

