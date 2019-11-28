#include <hash.h>
#include <list.h>
#include "threads/thread.h"

enum write_to 
{
    CODE, MMAP, DATA, SWAP
};

struct frame
{
    enum write_to write_to;
    void *where_to_write;
    uint32_t *write_size;
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

struct frame *frame_table_lookup (const void *paddr);

struct frame*make_frame (enum write_to write_to, void *where_to_write, uint32_t *write_size, void *paddr, void *page, bool pin);
void insert_to_hash_table (enum write_to write_to, struct frame *frame);

bool delete_page_from_frame_table(void *paddr, void *page, struct thread* owner);
bool delete_frame_from_frame_table(void *paddr, struct thread* owner);

struct list_elem *find_elem_in_page_list (struct list *page_list, void *page, struct thread* owner);