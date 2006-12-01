/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
** This file provide the memory allocator used internally.
** Currently you have two option, a wrapper to malloc/free or a static allocator.
*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "mempool_struct.h"

#define EMEMOA_MAGIC    0xDEAD5007

#ifdef DEBUG
#define EMEMOA_CHECK_MAGIC(Memory) \
        assert(Memory->magic == EMEMOA_MAGIC);
#else
#define EMEMOA_CHECK_MAGIC(Memory) ;
#endif

static void* (*base_malloc)(unsigned int size) = malloc;
static void  (*base_free)(void* ptr) = free;

void*
ememoa_memory_base_alloc (unsigned int size)
{
   return base_malloc (size);
}

void
ememoa_memory_base_free (void* ptr)
{
   base_free (ptr);
}

static struct ememoa_memory_base_s     *base_64m = NULL;

static void
ememoa_memory_base_remove_from_list (uint16_t index)
{
   uint16_t     prev = base_64m->chunks[index].prev;
   uint16_t     next = base_64m->chunks[index].next;

   if (prev != 0xFFFF)
     base_64m->chunks[prev].next = next;
   if (next != 0xFFFF)
     base_64m->chunks[next].prev = prev;
   if (base_64m->start == index)
     base_64m->start = next;
   if (base_64m->over == index)
     base_64m->over = prev;

   base_64m->chunks[index].prev = 0xFFFF;
   base_64m->chunks[index].next = 0xFFFF;
}

static void
ememoa_memory_base_insert_in_list (uint16_t index)
{
   uint16_t     length = base_64m->chunks[index].length;
   uint16_t     prev = 0xFFFF;
   uint16_t     next;

   if (base_64m->chunks[index].start == 0xFFFF)
     return ;

   for (next = base_64m->start; next != 0xFFFF && base_64m->chunks[next].length > length; next = base_64m->chunks[next].next)
     prev = next;

   assert (index != next);
   assert (index != prev);

   base_64m->chunks[index].next = next;
   base_64m->chunks[index].prev = prev;

   if (next != 0xFFFF)
     base_64m->chunks[next].prev = index;
   else
     base_64m->over = index;

   if (prev != 0xFFFF)
     base_64m->chunks[prev].next = index;
   else
     base_64m->start = index;
}

static uint16_t
ememoa_memory_base_merge_64m (uint16_t  one,
                              uint16_t  two)
{
   uint16_t     tmp;
   uint16_t     index;

   if (base_64m->chunks[one].length < base_64m->chunks[two].length)
     {
        tmp = one;
        one = two;
        two = tmp;
     }

   /* All page refering to 'two' now refere to 'one'. */
   for (index = base_64m->chunks[two].start;
        index != base_64m->chunks[two].end;
        ++index)
     base_64m->pages[index] = one;
   base_64m->pages[index] = one;

   if (base_64m->chunks[one].start < base_64m->chunks[two].start)
     base_64m->chunks[one].end = base_64m->chunks[two].end;
   else
     base_64m->chunks[one].start = base_64m->chunks[two].start;

   base_64m->chunks[one].length += base_64m->chunks[two].length;

   base_64m->chunks[two].start = 0xFFFF;
   base_64m->chunks[two].use = 0;
   if (base_64m->jump > two)
     base_64m->jump = two;

   return one;
}

static uint16_t
ememoa_memory_base_split_64m (uint16_t index, unsigned int length)
{
   uint16_t     splitted = 0xFFFF;

   if (base_64m->chunks[index].length != length)
     {
        uint16_t        i;

        while (!(base_64m->chunks[base_64m->jump].start == 0xFFFF
                 && base_64m->chunks[base_64m->jump].prev == 0xFFFF
                 && base_64m->chunks[base_64m->jump].next == 0xFFFF))
          base_64m->jump++;

        splitted = base_64m->jump;

        base_64m->chunks[splitted].length = base_64m->chunks[index].length - length;
        base_64m->chunks[splitted].end = base_64m->chunks[index].end;
        base_64m->chunks[splitted].start = base_64m->chunks[index].start + length;
        base_64m->chunks[splitted].next = 0xFFFF;
        base_64m->chunks[splitted].prev = 0xFFFF;
        base_64m->chunks[splitted].use = 0;

        for (i = base_64m->chunks[splitted].start;
             i != base_64m->chunks[splitted].end;
             ++i)
          base_64m->pages[i] = splitted;
        base_64m->pages[i] = splitted;

        base_64m->chunks[index].length = length;
        base_64m->chunks[index].end = base_64m->chunks[index].start + length - 1;
     }
   return splitted;
}

static void*
ememoa_memory_base_alloc_64m (unsigned int size)
{
   uint16_t     real = (size >> 12) + (size & 0xFFF ? 1 : 0);
   uint16_t     jump = base_64m->start;
   uint16_t     prev = 0xFFFF;

   while (jump != 0xFFFF && base_64m->chunks[jump].length > real)
     {
        prev = jump;
        jump = base_64m->chunks[jump].next;
     }

   if (prev != 0xFFFF)
     {
        uint16_t        splitted = ememoa_memory_base_split_64m (prev, real);

        if (splitted != 0xFFFF)
          ememoa_memory_base_insert_in_list (splitted);

        ememoa_memory_base_remove_from_list (prev);
        base_64m->chunks[prev].use = 1;

        return ((uint8_t*) base_64m->base) + (base_64m->chunks[prev].start << 12);
     }
   abort();
   return NULL;
}

static void
ememoa_memory_base_free_64m (void* ptr)
{
   unsigned int delta = ptr - base_64m->base;
   uint16_t     index = delta >> 12;
   uint16_t     chunk_index = base_64m->pages[index];
   uint16_t     prev_chunk_index;
   uint16_t     next_chunk_index;

   if (ptr == NULL)
     return ;

   assert (ptr > base_64m->base);

   prev_chunk_index = base_64m->pages[index - 1];
   next_chunk_index = base_64m->pages[base_64m->chunks[chunk_index].end + 1];

   if (index > 0 && prev_chunk_index < base_64m->chunks_count)
     if (base_64m->chunks[prev_chunk_index].use == 0)
       {
          ememoa_memory_base_remove_from_list(prev_chunk_index);
          chunk_index = ememoa_memory_base_merge_64m(chunk_index, prev_chunk_index);
       }

   if (base_64m->chunks[chunk_index].end < base_64m->chunks_count
       && next_chunk_index < base_64m->chunks_count)
     if (base_64m->chunks[next_chunk_index].use == 0)
       {
          ememoa_memory_base_remove_from_list(next_chunk_index);
          chunk_index = ememoa_memory_base_merge_64m(chunk_index, next_chunk_index);
       }

   ememoa_memory_base_insert_in_list (chunk_index);
   base_64m->chunks[chunk_index].use = 0;
}

int
ememoa_memory_base_init_64m (void* buffer, unsigned int size)
{
   struct ememoa_memory_base_s  *new_64m = buffer;
   unsigned int                 temp_size;

   if (!new_64m)
     return -1;

   temp_size = (size - sizeof (struct ememoa_memory_base_s)) >> 12;
   if (temp_size <= 1)
     return -1;

#ifdef DEBUG
   new_64m->magic = EMEMOA_MAGIC;
#endif
   new_64m->chunks = (struct ememoa_memory_base_chunck_s*) ((struct ememoa_memory_base_s*) new_64m + 1);
   new_64m->pages = (uint16_t*)((struct ememoa_memory_base_chunck_s*) new_64m->chunks + temp_size + 1);
   new_64m->base = ((uint16_t*) new_64m->pages + temp_size + 1);
   new_64m->start = 0;

   new_64m->chunks_count = (size
                            - sizeof (struct ememoa_memory_base_s)
                            - temp_size * (sizeof (struct ememoa_memory_base_chunck_s) + sizeof (uint16_t)))
     / 4096;


   memset (new_64m->chunks, 0xFF, sizeof (struct ememoa_memory_base_chunck_s) * temp_size);
   memset (new_64m->pages, 0, sizeof (uint16_t) * temp_size);

   new_64m->chunks[0].start = 0;
   new_64m->chunks[0].end = new_64m->chunks_count - 1;
   new_64m->chunks[0].length = new_64m->chunks_count;
   new_64m->chunks[0].next = 0xFFFF;
   new_64m->chunks[0].prev = 0xFFFF;
   new_64m->chunks[0].use = 0;
   new_64m->over = 0;
   new_64m->start = 0;
   new_64m->jump = 1;

   base_64m = new_64m;

   base_malloc = ememoa_memory_base_alloc_64m;
   base_free = ememoa_memory_base_free_64m;

   return 0;
}

struct ememoa_memory_base_resize_list_s*
ememoa_memory_base_resize_list_new (unsigned int size)
{
   struct ememoa_memory_base_resize_list_s      *tmp;

   if (size == 0)
     return NULL;

   tmp = ememoa_memory_base_alloc (sizeof (struct ememoa_memory_base_resize_list_s));

   assert(tmp != NULL);

   bzero (tmp, sizeof (struct ememoa_memory_base_resize_list_s));
   tmp->size = size;

#ifdef DEBUG
   tmp->magic = EMEMOA_MAGIC;
#endif

   return tmp;
}

void
ememoa_memory_base_resize_list_clean (struct ememoa_memory_base_resize_list_s*  base)
{
   struct ememoa_memory_base_resize_list_item_s *start;
   struct ememoa_memory_base_resize_list_item_s *next;
#ifdef DEBUG
   unsigned int                                 warned = 0;
#endif

   EMEMOA_CHECK_MAGIC(base);

   for (start = base->start;
        start != NULL;
        start = next)
     {
        next = start->next;

#ifdef DEBUG
        if (!warned && start->used != 0xFFFFFFFF)
          fprintf(stderr, "Warning some element where not freed from this list.\n");
#endif

        ememoa_memory_base_free (start->pool);
        ememoa_memory_base_free (start);
     }

#ifdef DEBUG
   base->magic = 0;
#endif
   ememoa_memory_base_free (base);
}

int
ememoa_memory_base_resize_list_new_item (struct ememoa_memory_base_resize_list_s *base)
{
   struct ememoa_memory_base_resize_list_item_s *start;
   unsigned int                                 j;

   EMEMOA_CHECK_MAGIC(base);

   for (j = 0, start = base->start;
        start != NULL && start->used == 0;
        start = start->next, ++j)
     ;

   if (start)
     {
        int     mask = 1;
        int     index;

        index = ffs (start->used) - 1;
        mask <<= index;
        start->used &= ~mask;
        base->delivered++;

        return (base->count - j - 1) * 32 + index;
     }

   start = ememoa_memory_base_alloc (sizeof (struct ememoa_memory_base_resize_list_s));

   assert(start != NULL);

   start->used = 0xFFFFFFFE;
   start->next = base->start;
   start->prev = NULL;
   if (base->start)
     base->start->prev = start;
   else
     base->end = start;

   start->pool = ememoa_memory_base_alloc (base->size * 32);
#ifdef DEBUG
   memset (start->pool, 43, base->size * 32);
#endif

   assert(start->pool);

   base->start = start;
   base->count++;
   base->delivered++;

   return (base->count - 1) * 32;
}

void*
ememoa_memory_base_resize_list_get_item (struct ememoa_memory_base_resize_list_s *base, int index)
{
   struct ememoa_memory_base_resize_list_item_s *start;
   unsigned int                                 i;
   unsigned int                                 j;

   EMEMOA_CHECK_MAGIC(base);

   j = index >> 5;
   i = index & 0x1F;

   if (j > (base->count >> 1))
     for (start = base->end;
          j > 0 && start != NULL;
          --j, start = start->prev)
       ;
   else
     for (start = base->start, j = base->count - j - 1;
          j > 0 && start != NULL;
          --j, start = start->next)
       ;

   if (j == 0 && start)
     return (void*) ((uint8_t*) start->pool + i * base->size);

   return NULL;
}

void
ememoa_memory_base_resize_list_back (struct ememoa_memory_base_resize_list_s *base, int index)
{
   struct ememoa_memory_base_resize_list_item_s *start;
   unsigned int                                 i;
   unsigned int                                 j;

   EMEMOA_CHECK_MAGIC(base);

   j = index >> 5;
   i = index & 0x1F;

   if (j > (base->count >> 2))
     for (start = base->end;
          j > 0 && start != NULL;
          --j, start = start->prev)
       ;
   else
     for (start = base->start, j = base->count - j - 1;
          j > 0 && start != NULL;
          --j, start = start->next)
       ;

   if (j == 0 && start)
     {
#ifdef DEBUG
        memset (start->pool + base->size * i, 44, base->size);
#endif
        start->used |= 1 << i;
        base->delivered--;
     }
}

int
ememoa_memory_base_resize_list_garbage_collect (struct ememoa_memory_base_resize_list_s *base)
{
   struct ememoa_memory_base_resize_list_item_s *start;
   struct ememoa_memory_base_resize_list_item_s *prev;
   int                                          result = -1;

   EMEMOA_CHECK_MAGIC(base);

   start = base->start;

   if (start)
     for (prev = start, start = start->next;
          start != NULL;
          prev = start, start = start->next)
       if (start->used == 0xFFFFFFFF)
         {
            prev->next = start->next;
            if (start->next)
              start->next->prev = prev;
            else
              base->end = prev;
            ememoa_memory_base_free (start->pool);
            ememoa_memory_base_free (start);
            start = prev->next;
            result = 0;
            if (!start)
              break;
         }

   if (base->start)
     if (base->start->used == 0xFFFFFFFF)
       {
          start = base->start;
          base->start = start->next;
          if (start->next == NULL)
            base->end = NULL;
          ememoa_memory_base_free (start->pool);
          ememoa_memory_base_free (start);
          result = 0;
       }

   return result;
}

int
ememoa_memory_base_resize_list_walk_over (struct ememoa_memory_base_resize_list_s *base,
                                          int start,
                                          int end,
                                          int (*fct)(void *ctx, int index, void *data),
                                          void *ctx)
{
   struct ememoa_memory_base_resize_list_item_s *l;
   unsigned int                                 mask;
   int                                          j;
   int                                          i;
   int                                          result = 0;

   EMEMOA_CHECK_MAGIC(base);

   j = start >> 5;
   i = start & 0x1F;

   if (end < 0)
     end = -1;

   for (l = base->end;
        l != NULL && j > 0;
        l = l->prev, --j)
     ;

   for (j = start & 0xFFFFFFE0;
        l != NULL && j < (end & 0x7FFFFFE0);
        j += 32, l = l->prev)
     for (mask = ~l->used, i = ffs(mask) - 1, mask >>= i;
          mask;
          ++i, mask >>= 1)
       if (mask & 1)
         result += fct (ctx, j + i, l->pool + i * base->size);

   if (l != NULL)
     for (mask = ~l->used & ((end & 0x1F) - 1), i = ffs(mask) - 1, mask >>= i;
          mask;
          ++i, mask >>= 1)
       if (mask & 1)
         result += fct (ctx, j + i, l->pool + i * base->size);

   return result;
}

void*
ememoa_memory_base_resize_list_search_over (struct ememoa_memory_base_resize_list_s *base,
                                            int start,
                                            int end,
                                            int (*fct)(void *ctx, int index, void *data),
                                            void *ctx,
                                            int *index)
{
   struct ememoa_memory_base_resize_list_item_s *l;
   unsigned int                                 mask;
   int                                          j;
   int                                          i;

   EMEMOA_CHECK_MAGIC(base);

   j = start >> 5;
   i = start & 0x1F;

   if (end < 0)
     end = -1;

   for (l = base->end;
        l != NULL && j > 0;
        l = l->prev, --j)
     ;

   for (j = start & 0xFFFFFFE0;
        l != NULL && j < (end & 0x7FFFFFE0);
        j += 32, l = l->prev)
     for (mask = ~l->used, i = ffs(mask) - 1, mask >>= i;
          mask;
          ++i, mask >>= 1)
       if (mask & 1)
         if (fct (ctx, j + i, l->pool + i * base->size))
           {
              if (index)
                *index = j + i;
              return l->pool + i * base->size;
           }

   if (l != NULL)
     for (mask = ~l->used & ((end & 0x1F) - 1), i = ffs(mask) - 1, mask >>= i;
          mask;
          ++i, mask <<= 1)
       if (mask & 1)
         if (fct (ctx, j + i, l->pool + i * base->size))
           {
              if (index)
                *index = j + i;
              return l->pool + i * base->size;
           }

   if (index)
     *index = 0;
   return NULL;
}

