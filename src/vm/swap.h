#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

void swap_init (void);
size_t put_to_swap (struct frame *frame);
void load_from_swap (struct spage *spage, struct frame *f);
void clear_swap_slot (int n);
#endif
