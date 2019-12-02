#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/page.h"
enum write_to 
{
    CODE_F, MMAP_F, DATA_F, STACK_F
};

struct frame
{
    enum write_to write_to;
    int32_t where_to_write;
    uint32_t write_size;
    void *kpage;
    bool pin;
    struct list upage_list;
    struct hash_elem hash_elem;
};

struct upage_for_frame_table
{
    void *upage;
    struct thread *owner;
    struct list_elem list_elem;
};

void frame_table_init (void);

struct frame* make_frame (enum write_to write_to, int32_t where_to_write, uint32_t write_size, void *kpage, void *upage, bool pin);

bool insert_to_frame_table (enum palloc_flags flags, struct frame *frame);
bool delete_upage_from_frame_table(void *kpage, void *upage, struct thread* owner);
bool delete_frame_from_frame_table (void *kpage);

enum write_to convert_read_from_to_write_to (enum read_from read_from);
#endif
