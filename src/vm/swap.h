#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "vm/frame.h"
#include "vm/page.h"

void swap_init (int n);
size_t put_to_swap (struct frame *frame);
void load_from_swap (struct frame *frame);
void clear_swap_table (int n);
#endif
