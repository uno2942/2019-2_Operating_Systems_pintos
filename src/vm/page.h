#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>

enum read_from 
{
    CODE_P, MMAP_P, DATA_P, SWAP_P
};

struct spage
{
    enum read_from read_from;
    void *where_to_read;        // If READ_FROM == SWAP, then it indicates slot #.
    uint32_t read_size;
    void *page;
    struct hash_elem hash_elem;
};

void supplemental_page_table_init (struct hash* sp_table);
struct hash_elem *supplemental_page_table_lookup (struct hash* sp_table, void *page);
void insert_to_supplemental_page_table (struct hash* sp_table, struct spage* page);
void delete_from_supplemental_page_table (struct hash* sp_table, void *page);
void clear_supplemental_page_table (struct hash* sp_table);
#endif