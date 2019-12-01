#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
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
    void *paddr;
    bool pin;
    struct list page_list;
    struct hash_elem hash_elem;
};

struct page_for_frame_table
{
    void *page;
    struct thread *owner;
    struct list_elem list_elem;
};

void frame_table_init (void);
struct frame *frame_table_lookup (void *paddr);
struct frame* make_frame (enum write_to write_to, int32_t where_to_write, uint32_t write_size, void *paddr, void *page, bool pin);
void insert_to_frame_table (struct frame *frame);
bool delete_page_from_frame_table(void *paddr, void *page, struct thread* owner);
bool delete_frame_from_frame_table (void *paddr);
struct list_elem *find_elem_in_page_list (struct list *page_list, void *page, struct thread* owner);
enum write_to convert_read_from_to_write_to (enum read_from read_from);
#endif
