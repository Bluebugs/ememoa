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

void* (*ememoa_memory_base_alloc)(unsigned int size) = malloc;
void  (*ememoa_memory_base_free)(void* ptr) = free;
void* (*ememoa_memory_base_realloc)(void* ptr, unsigned int size) = realloc;

/**
 * @defgroup Ememoa_Mempool_Base_64m Static buffer allocator
 *
 */

/**
 * Global context for the static buffer allocator.
 * @ingroup Ememoa_Mempool_Base_64m
 */
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
   if (base_64m->chunks[index].length != length)
     {
        struct ememoa_memory_base_chunck_s      a;
        struct ememoa_memory_base_chunck_s      b;
        uint16_t                                i;
        uint16_t                                splitted;

        while (!(base_64m->chunks[base_64m->jump].start == 0xFFFF
                 && base_64m->chunks[base_64m->jump].prev == 0xFFFF
                 && base_64m->chunks[base_64m->jump].next == 0xFFFF))
          base_64m->jump++;

        splitted = base_64m->jump++;
        ememoa_memory_base_remove_from_list (index);

        a = base_64m->chunks[index];

        b.length = a.length - length;
        b.end = a.end;
        b.start = a.start + length;
        b.next = 0xFFFF;
        b.prev = 0xFFFF;
        b.use = 0;

        a.length = length;
        a.end = a.start + length - 1;
        a.use = 1;

        if (a.length < b.length)
          {
             base_64m->chunks[index] = b;
             base_64m->chunks[splitted] = a;
             ememoa_memory_base_insert_in_list (index);
          }
        else
          {
             base_64m->chunks[index] = a;
             base_64m->chunks[splitted] = b;
             ememoa_memory_base_insert_in_list (splitted);
          }

        for (i = base_64m->chunks[splitted].start;
             i != base_64m->chunks[splitted].end;
             ++i)
          base_64m->pages[i] = splitted;
        base_64m->pages[i] = splitted;

        return splitted;
     }
   return 0xFFFF;
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
        uint16_t        allocated;
        uint16_t        empty;

        allocated = base_64m->chunks[prev].use == 1 ? prev : splitted;
        empty = base_64m->chunks[prev].use == 1 ? splitted : prev;

        return ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12);
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

static void*
ememoa_memory_base_realloc_64m (void* ptr, unsigned int size)
{
   void*        tmp;
   unsigned int delta = ptr - base_64m->base;
   uint16_t     real = (size >> 12) + (size & 0xFFF ? 1 : 0);
   uint16_t     index = delta >> 12;
   uint16_t     chunk_index = base_64m->pages[index];
   uint16_t     next_chunk_index;

   if (ptr == NULL)
     return ememoa_memory_base_alloc_64m(size);

   assert (ptr > base_64m->base);

   if (real <= base_64m->chunks[chunk_index].length)
     return ptr;

   next_chunk_index = base_64m->pages[base_64m->chunks[chunk_index].end + 1];

   if (base_64m->chunks[next_chunk_index].use == 0)
     if (real <= base_64m->chunks[next_chunk_index].length + base_64m->chunks[chunk_index].length)
       {
          uint16_t      splitted;
          uint16_t      allocated;
          uint16_t      empty;

          ememoa_memory_base_remove_from_list(next_chunk_index);
          chunk_index = ememoa_memory_base_merge_64m(chunk_index, next_chunk_index);
          splitted = ememoa_memory_base_split_64m (chunk_index, real);

          allocated = base_64m->chunks[chunk_index].use == 1 ? chunk_index : splitted;
          empty = base_64m->chunks[chunk_index].use == 1 ? splitted : chunk_index;

          if (splitted != 0xFFFF)
            ememoa_memory_base_insert_in_list (empty);

          ememoa_memory_base_remove_from_list (allocated);

          return ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12);
       }

   tmp = ememoa_memory_base_alloc_64m(size);
   if (!tmp)
     return NULL;

   memcpy(tmp, ptr, base_64m->chunks[chunk_index].length << 12);
   ememoa_memory_base_free_64m(ptr);

   return tmp;
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

   ememoa_memory_base_alloc = ememoa_memory_base_alloc_64m;
   ememoa_memory_base_free = ememoa_memory_base_free_64m;
   ememoa_memory_base_realloc = ememoa_memory_base_realloc_64m;

   return 0;
}

/**
 * @defgroup Ememoa_Mempool_Base_Resize_List Function enabling manipulation of array with linked list properties.
 *
 */

struct ememoa_memory_base_resize_list_s*
ememoa_memory_base_resize_list_new (unsigned int size)
{
   struct ememoa_memory_base_resize_list_s      *tmp;

   if (size == 0)
     return NULL;

   tmp = ememoa_memory_base_alloc (sizeof (struct ememoa_memory_base_resize_list_s));

   if (tmp == NULL)
     return NULL;

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
   EMEMOA_CHECK_MAGIC(base);

   ememoa_memory_base_free (base->pool);
   ememoa_memory_base_free (base->bitmap);

#ifdef DEBUG
   if (base->actif != 0)
     fprintf(stderr, "Warning some element where not freed from this list.\n");
#endif

#ifdef DEBUG
   base->magic = 0;
#endif

   ememoa_memory_base_free (base);
}

int
ememoa_memory_base_resize_list_new_item (struct ememoa_memory_base_resize_list_s *base)
{
   int                  i;

   EMEMOA_CHECK_MAGIC(base);

   if (base->count < base->actif + 1)
     {
        unsigned int    count;
        uint32_t        *tmp_bitmap;
        void            *tmp_pool;

        count = base->count + 32;

        tmp_pool = ememoa_memory_base_realloc (base->pool, count * base->size);
        if (!tmp_pool)
          return -1;
        base->pool = tmp_pool;

        tmp_bitmap = ememoa_memory_base_realloc (base->bitmap, (count >> 5) * sizeof (uint32_t));
        if (!tmp_bitmap)
          return -1;
        base->bitmap = tmp_bitmap;

        base->bitmap[base->count >> 5] = 0xFFFFFFFF;

#ifdef DEBUG
        memset ((uint8_t*) tmp_pool + base->count * base->size, 43, base->size * 32);
#endif
        base->count = count;

     }

   for (; base->jump < (base->count >> 5) && base->bitmap[base->jump] == 0; ++base->jump)
     ;

   assert (base->jump < (base->count >> 5));

   i = ffs(base->bitmap[base->jump]) - 1;

   assert (i >= 0 && i < 32);

   base->bitmap[base->jump] &= ~(1 << i);
   base->actif++;

   return (base->jump << 5) + i;
}

void*
ememoa_memory_base_resize_list_get_item (struct ememoa_memory_base_resize_list_s *base, int index)
{
   EMEMOA_CHECK_MAGIC(base);

   if (index < 0)
     return NULL;

   return (void*) ((uint8_t*) base->pool + index * base->size);
}

void
ememoa_memory_base_resize_list_back (struct ememoa_memory_base_resize_list_s *base, int index)
{
   unsigned int                                 shift;
   unsigned int                                 i;

   EMEMOA_CHECK_MAGIC(base);

   if (index < 0)
     return ;

   shift = index >> 5;
   i = index & 0x1F;

   base->bitmap[shift] |= (1 << i);
   base->actif--;

   if (shift < base->jump)
     base->jump = shift;

#ifdef DEBUG
   memset ((uint8_t*) base->pool + base->size * index, 44, base->size);
#endif
}

int
ememoa_memory_base_resize_list_garbage_collect (struct ememoa_memory_base_resize_list_s *base)
{
   uint32_t     *tmp_bitmap;
   void         *tmp_pool;
   unsigned int count;

   EMEMOA_CHECK_MAGIC(base);

   count = base->count;

   for (; base->count > 0 && base->bitmap[(base->count >> 5)] == 0xFFFFFFFF; base->count -= 32)
     ;

   tmp_pool = ememoa_memory_base_realloc (base->pool, base->count * base->size);
   if (!tmp_pool)
     return -1;
   base->pool = tmp_pool;

   tmp_bitmap = ememoa_memory_base_realloc (base->bitmap, (base->count >> 5) * sizeof (uint32_t));
   if (!tmp_bitmap)
     return -1;
   base->bitmap = tmp_bitmap;

   return count != base->count;
}

int
ememoa_memory_base_resize_list_walk_over (struct ememoa_memory_base_resize_list_s *base,
                                          int start,
                                          int end,
                                          int (*fct)(void *ctx, int index, void *data),
                                          void *ctx)
{
   int          bitmap;
   int          first;
   int          end_shift;
   int          end_i;
   int          shift;
   int          i;
   int          result = 0;

   EMEMOA_CHECK_MAGIC(base);

   i = start & 0x1F;

   if (end < 0)
     end = base->count - 1;

   if (end == -1)
     return 0;

   end_shift = end >> 5;
   end_i = end & 0x1F;

   for (shift = start >> 5; shift < end_shift; ++shift)
     {
        bitmap = ~base->bitmap[shift];
        first = ffs(bitmap) - 1;

        i = i > first ? i : first;
        for (bitmap >>= i; i < 32; ++i, bitmap >>= 1, ++start)
          if (bitmap & 0x1)
            result += fct (ctx, start, base->pool + start * base->size);
        i = 0;
     }

   bitmap = ~base->bitmap[shift];
   for (bitmap >>= i; i < end_i; ++i, bitmap >>= 1, ++start)
     if (bitmap & 0x1)
       result += fct (ctx, start, base->pool + start * base->size);

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
   int          bitmap;
   int          first;
   int          end_shift;
   int          end_i;
   int          shift;
   int          i;

   EMEMOA_CHECK_MAGIC(base);

   i = start & 0x1F;

   if (end < 0)
     end = base->count - 1;

   if (end == -1)
     return NULL;

   end_shift = end >> 5;
   end_i = end & 0x1F;

   for (shift = start >> 5; shift < end_shift; ++shift)
     {
        bitmap = ~base->bitmap[shift];
        first = ffs(bitmap) - 1;

        i = i > first ? i : first;
        for (bitmap >>= i; i < 32; ++i, bitmap >>= 1, ++start)
          if (bitmap & 0x1)
            if (fct (ctx, start, base->pool + start * base->size))
              goto found;
        i = 0;
     }

   bitmap = ~base->bitmap[shift];
   for (bitmap >>= i; i < end_i; ++i, bitmap >>= 1, ++start)
     if (bitmap & 0x1)
       if (fct (ctx, start, base->pool + start * base->size))
         goto found;

   if (index)
     *index = 0;
   return NULL;

  found:
   if (index)
     *index = start;
   return base->pool + start * base->size;
}

