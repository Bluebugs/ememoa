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

#include "ememoa_memory_base.h"

#define EMEMOA_MAGIC    0xDEAD5007

#ifdef DEBUG
#define EMEMOA_CHECK_MAGIC(Memory) \
        assert(Memory->magic == EMEMOA_MAGIC);
#else
#define EMEMOA_CHECK_MAGIC(Memory) ;
#endif

void*
ememoa_memory_base_alloc (unsigned int size)
{
   return malloc (size);
}

void
ememoa_memory_base_free (void* ptr)
{
   free (ptr);
}

struct ememoa_memory_base_resize_list_s*
ememoa_memory_base_resize_list_new (unsigned int size)
{
   struct ememoa_memory_base_resize_list_s      *tmp;

   if (size == 0)
     return NULL;

   tmp = ememoa_memory_base_alloc(sizeof (struct ememoa_memory_base_resize_list_s));

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

