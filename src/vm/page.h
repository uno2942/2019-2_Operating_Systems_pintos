#include <hash.h>

enum write_to 
{
    FILE, SWAP
};

struct spage
{
    enum read_from read_from;
    void *where_to_read;        // If READ_FROM == SWAP, then it indicates slot #.
    uint32_t read_size;
    void *page;
    void *frame;
    struct hash_elem hash_elem;
};

void supplemental_page_table_init (void);
struct hash_elem *supplemental_page_table_lookup (struct hash* sp_table,const void *page);
void insert_to_supplemental_page_table (struct hash* sp_table, struct spage* page);
void delete_from_supplemental_page_table (struct hash* sp_table, void *page);