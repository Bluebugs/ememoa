/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef         EMEMOA_MEMORY_BASE_H__
# define        EMEMOA_MEMORY_BASE_H__

struct ememoa_memory_base_resize_list_item_s
{
   void                                         *pool;
   unsigned int                                 used;

   struct ememoa_memory_base_resize_list_item_s *next;
   struct ememoa_memory_base_resize_list_item_s *prev;
};

struct ememoa_memory_base_resize_list_s
{
#ifdef DEBUG
   unsigned int                                 magic;
#endif

   unsigned int                                 count;
   unsigned int                                 delivered;
   unsigned int                                 size;

   struct ememoa_memory_base_resize_list_item_s *start;
   struct ememoa_memory_base_resize_list_item_s *end;
};

/* Direct use of this two function is most of the time a bad idea. */
void*           ememoa_memory_base_alloc (unsigned int size);
void            ememoa_memory_base_free (void *ptr);
int             ememoa_memory_base_init_64m (void* buffer, unsigned int size);

struct ememoa_memory_base_resize_list_s*        ememoa_memory_base_resize_list_new (unsigned int size);
void    ememoa_memory_base_resize_list_clean (struct ememoa_memory_base_resize_list_s*  base);
int     ememoa_memory_base_resize_list_new_item (struct ememoa_memory_base_resize_list_s *base);
void*   ememoa_memory_base_resize_list_get_item (struct ememoa_memory_base_resize_list_s *base, int index);
void    ememoa_memory_base_resize_list_back (struct ememoa_memory_base_resize_list_s *base, int index);
int     ememoa_memory_base_resize_list_walk_over (struct ememoa_memory_base_resize_list_s *base,
                                                  int start,
                                                  int end,
                                                  int (*fct)(void *ctx, int index, void *data),
                                                  void *ctx);
void*   ememoa_memory_base_resize_list_search_over (struct ememoa_memory_base_resize_list_s *base,
                                                    int start,
                                                    int end,
                                                    int (*fct)(void *ctx, int index, void *data),
                                                    void *ctx,
                                                    int *index);

/* Be carefull when you call this function, you will loose all index mapping after this call. */
int     ememoa_memory_base_resize_list_garbage_collect (struct ememoa_memory_base_resize_list_s *base);

#endif          /* EMEMOA_MEMORY_BASE_H__ */
