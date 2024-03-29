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
#include "config.h"

#include "mempool_struct.h"

#define EMEMOA_MAGIC    0xDEAD5007

#ifdef DEBUG
#define EMEMOA_CHECK_MAGIC(Memory) \
        assert(Memory->magic == EMEMOA_MAGIC);
#else
#define EMEMOA_CHECK_MAGIC(Memory) ;
#endif

static int total = 0;

#ifdef HAVE_PTHREAD
static pthread_mutex_t lockit = PTHREAD_MUTEX_INITIALIZER;

#define LK(Lock) pthread_mutex_lock(&Lock);
#define ULK(Lock) pthread_mutex_unlock(&Lock);

#else

#define LK(Lock)
#define ULK(Lock)

#endif

void* (*ememoa_memory_base_alloc)(size_t size) = malloc;
void  (*ememoa_memory_base_free)(void* ptr) = free;
void* (*ememoa_memory_base_realloc)(void* ptr, size_t size) = realloc;

/**
 * @defgroup Ememoa_Mempool_Base_64m Static buffer allocator.
 *
 */

/**
 * Global context for the static buffer allocator.
 * @ingroup Ememoa_Mempool_Base_64m
 */
static struct ememoa_memory_base_s     *base_64m = NULL;

/**
 * Remove an item from the base_64m free block list.
 *
 * @param       index   Item to be removed.
 * @return	Will never fail.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
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

/**
 * Insert an item in the base_64m free block list.
 *
 * @param       index   Item to be inserted.
 * @return	Will break completely if the item is already in the list.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
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

/**
 * Merge two chunk of memory together. Choose the index of the resulting
 * chunk as the one requiring less effort for committing the change. No
 * requirement on parameters order or any other characteristic exist.
 *
 * @param       one     First part of the chunk to be merged.
 * @param       two     Second part of the chunk to be merged.
 * @return	Index of the new chunk.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
static uint16_t
ememoa_memory_base_merge_64m (uint16_t  one,
                              uint16_t  two)
{
   uint16_t     index;
   uint16_t     tmp;

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

/**
 * Split a chunk in two, allocating a new one on the fly and doing as few as possible
 * memory update. The new allocated chunk could be used as the left or right part of
 * the splitted chunk depending on the fastest strategie. You will need to guess by your
 * self what was our choice.
 *
 * @param       index   The item to split.
 * @param       length  The required size for one of the two resulting chunk.
 * @return	0xFFFF, if the chunk already has the right size otherwise the new allocated chunk.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
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

/**
 * Just allocate like malloc a new memory chunk from the static buffer.
 *
 * @param       size    The asked size.
 * @return	NULL if not enough memory, or a correct pointer otherwise.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
static void*
ememoa_memory_base_alloc_64m (size_t size)
{
   uint16_t     real = (size >> 12) + (size & 0xFFF ? 1 : 0);
   uint16_t     jump = base_64m->start;
   uint16_t     prev = 0xFFFF;

   LK(lockit);

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

        /* Guess who is who */
        allocated = base_64m->chunks[prev].use == 1 ? prev : splitted;
        empty = base_64m->chunks[prev].use == 1 ? splitted : prev;

	total += real;
#ifdef ALLOC_REPORT
	fprintf(stderr, "alloc %i(%i) [%i] => %p\n", real << 12, size, total << 12, ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12));
#endif
	ULK(lockit);

        return ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12);
     }

   ULK(lockit);

   return NULL;
}

/**
 * Just free like free a previously allocated by ememoa_memory_base_alloc_64m memory chunk.
 *
 * @param       ptr     Pointer to be freed.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
static void
ememoa_memory_base_free_64m (void* ptr)
{
   unsigned int delta = ptr - base_64m->base;
   uint16_t     index;
   uint16_t     chunk_index;
   uint16_t     prev_chunk_index;
   uint16_t     next_chunk_index;

   if (ptr == NULL)
     return ;

   assert (ptr > base_64m->base);

   LK(lockit);

   index = delta >> 12;
   chunk_index = base_64m->pages[index];

   total -= base_64m->chunks[chunk_index].length;
#ifdef ALLOC_REPORT
   fprintf(stderr, "free %i [%i] => %p\n", base_64m->chunks[chunk_index].length, total << 12, ptr);
#endif

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

   ULK(lockit);
}

/**
 * Just resize a memory chunk like realloc. Will not resize block to a smaller size.
 *
 * @param       ptr     Pointer to the current pointer allocated by ememoa_memory_base_alloc_64m.
 * @param       size    The new asked size.
 * @return	NULL if not enough memory, or a correct pointer otherwise.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
static void*
ememoa_memory_base_realloc_64m (void* ptr, size_t size)
{
   void*        tmp;
   unsigned int delta = ptr - base_64m->base;
   uint16_t     real = (size >> 12) + (size & 0xFFF ? 1 : 0);
   uint16_t     index;
   uint16_t     chunk_index;
   uint16_t     next_chunk_index;

   if (ptr == NULL)
     return ememoa_memory_base_alloc_64m(size);

   assert (ptr > base_64m->base);

   index = delta >> 12;
   chunk_index = base_64m->pages[index];

   /* FIXME: Not resizing when the size is big enough */
   if (real <= base_64m->chunks[chunk_index].length)
     return ptr;

   LK(lockit);

   next_chunk_index = base_64m->pages[base_64m->chunks[chunk_index].end + 1];

   if (base_64m->chunks[next_chunk_index].use == 0)
     if (real <= base_64m->chunks[next_chunk_index].length + base_64m->chunks[chunk_index].length)
       {
          uint16_t      splitted;
          uint16_t      allocated;
	  int           tmp;

	  total -= base_64m->chunks[chunk_index].length;
	  tmp = base_64m->chunks[chunk_index].length;

	  ememoa_memory_base_remove_from_list(next_chunk_index);
          chunk_index = ememoa_memory_base_merge_64m(chunk_index, next_chunk_index);
          splitted = ememoa_memory_base_split_64m (chunk_index, real);

          allocated = base_64m->chunks[chunk_index].use == 1 ? chunk_index : splitted;

          ememoa_memory_base_remove_from_list (allocated);

	  total += real;
#ifdef ALLOC_REPORT
	  fprintf(stderr, "realloc %i(%i) [%i] => %p\n", (real - tmp) << 12, size, total << 12, ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12));
#endif

	  ULK(lockit);

          return ((uint8_t*) base_64m->base) + (base_64m->chunks[allocated].start << 12);
       }

   ULK(lockit);

   tmp = ememoa_memory_base_alloc_64m(size);
   if (!tmp)
     return NULL;

   memcpy(tmp, ptr, base_64m->chunks[chunk_index].length << 12);
   ememoa_memory_base_free_64m(ptr);

   return tmp;
}

/**
 * Switch all malloc/realloc/free operation of ememoa to static buffer allocation. You must call
 * this function before using any other ememoa operation.
 *
 * @param       buffer  The static buffer from which pointer will be given.
 * @param       size    The new asked size.
 * @return	NULL if not enough memory, or a correct pointer otherwise.
 * @ingroup	Ememoa_Mempool_Base_64m
 */
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
#define RESIZE_POOL_SIZE 128
struct ememoa_memory_base_resize_list_pool_s
{
   struct ememoa_memory_base_resize_list_pool_s *next;

   unsigned int					 count;
#ifdef USE64
   uint64_t					 map[2];
#else
   uint32_t					 map[4];
#endif
   struct ememoa_memory_base_resize_list_s	 array[RESIZE_POOL_SIZE];
};

static struct ememoa_memory_base_resize_list_pool_s *resize_pool = NULL;

/**
 * Allocate a new resizable list (it's an array now). It currently use to much memory
 * when using the base_64m allocator, it could be fixed if really usefull (Not high priority at this time).
 *
 * @param       size    items size inside the list.
 * @return	Will return a pointer to the base array.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
struct ememoa_memory_base_resize_list_s*
ememoa_memory_base_resize_list_new (unsigned int size)
{
   struct ememoa_memory_base_resize_list_s      *tmp;
   struct ememoa_memory_base_resize_list_pool_s *over;
#ifdef USE64
   uint64_t					 mask = 1;
#else
   uint32_t					 mask = 1;
#endif
   unsigned int					 i;
   int						 pos;

   if (size == 0)
     return NULL;

   for (over = resize_pool; over && over->count == RESIZE_POOL_SIZE; over = over->next)
     ;

   if (!over)
     {
	over = ememoa_memory_base_alloc (sizeof (struct ememoa_memory_base_resize_list_pool_s));
	if (!over) return NULL;

	over->next = resize_pool;
	over->count = 0;

#ifdef USE64
	for (i = 0; i < 2; ++i)
#else
        for(i = 0; i < 4; ++i)
#endif
	  over->map[i] = ~0;

	resize_pool = over;
     }

#ifdef USE64
   for (i = 0, pos = 0; i < 2 && pos == 0; ++i)
#else
   for (i = 0, pos = 0; i < 4 && pos == 0; ++i)
#endif
#ifdef USE64
     pos = ffsll (over->map[i]);
#else
     pos = ffs (over->map[i]);
#endif

   over->count++;
   pos--;
   i--;

#ifdef USE64
   assert(i >= 0 && i < 2);
   assert(pos >= 0 && pos < 64);
   tmp = over->array + pos + i * 64;
#else
   assert(i < 4);
   assert(pos >= 0 && pos < 32);
   tmp = over->array + pos + i * 32;
#endif

   mask <<= pos;
   over->map[i] &= ~mask;

   bzero (tmp, sizeof (struct ememoa_memory_base_resize_list_s));
   tmp->size = size;

#ifdef DEBUG
   tmp->magic = EMEMOA_MAGIC;
#endif

   return tmp;
}

/**
 * Clean a list and all it's item.
 *
 * @param       base    List
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
void
ememoa_memory_base_resize_list_clean (struct ememoa_memory_base_resize_list_s*  base)
{
   struct ememoa_memory_base_resize_list_pool_s *over;
   int index;

   if (!base)
     return ;

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

   for (over = resize_pool;
	over && !(over->array <= base && base < over->array + RESIZE_POOL_SIZE);
	over = over->next)
     ;

   assert(over != NULL);

   index = base - over->array;

#ifdef USE64
   over->map[index / 64] |= (1 << (index % 64));
#else
   over->map[index / 32] |= (1 << (index % 32));
#endif
}

/**
 * Allocate a new item in the list "base".
 *
 * @param       base    Pointer to a valid and activ list.
 * @return	Will return the new item index.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
int
ememoa_memory_base_resize_list_new_item (struct ememoa_memory_base_resize_list_s *base)
{
   int                  i;

   if (base == NULL)
     return -1;

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

/**
 * Allocate a set of new items in the list "base".
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       count   Number of item to return.
 * @return	Will return the base item index.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
int
ememoa_memory_base_resize_list_new_items (struct ememoa_memory_base_resize_list_s *base, int count)
{
   uint32_t	map;
   uint32_t	mask;
   int		first;
   int		index;
   unsigned int	i;

   if (base == NULL)
     return -1;

   EMEMOA_CHECK_MAGIC(base);

   if (base->count < base->actif + count)
     {
	unsigned int	total;
        uint32_t        *tmp_bitmap;
        void            *tmp_pool;

	total = base->count + (((count >> 5) + 1) << 5);

	tmp_pool = ememoa_memory_base_realloc (base->pool, total * base->size);
	if (!tmp_pool)
	  return -1;
	base->pool = tmp_pool;

	tmp_bitmap = ememoa_memory_base_realloc (base->bitmap, (total >> 5) * sizeof (uint32_t));
	if (!tmp_bitmap)
	  return -1;
	base->bitmap = tmp_bitmap;

	for (i = base->count >> 5; i < total >> 5; ++i)
	  tmp_bitmap[i] = 0xFFFFFFFF;

#ifdef DEBUG
        memset ((uint8_t*) tmp_pool + base->count * base->size, 43, base->size * (total - base->count));
#endif

	base->count = total;
     }

   /* First lookup for the first chunk with empty slot. */
   for (i = base->jump; i < (base->count >> 5) && base->bitmap[i] == 0; ++i)
     ;

   /* Check if we are at the end. */
   assert (i < (base->count >> 5));

   /* Lookup start on first empty chunk. */
   map = base->bitmap[i];
   index = i << 5;

   first = 0;
   do {
      int pos;

      /* Look in the current map for available chunk. */
      pos = ffs(map);
      if (pos > 0)
	{
	   uint32_t inv;
	   int jump;
	   int nbits;

	   /* nbits = count bit set in map starting from pos */
	   if (pos == 32) inv = 0;
	   else inv = ~map & ~((1 << pos) - 1);

	   jump = ffs(inv);
	   pos = pos - 1;
	   nbits = jump - pos;
	   while (jump == 0 && i < (base->count >> 5))
	     {
		map = base->bitmap[++i];
		inv = ~map;
		jump = ffs(inv);
		nbits += 32;
	     }

	   if (nbits >= count)
	     {
		base->actif += count;
		/* FIXME: Later improve this, but it will ok for the time being. */
		while (count)
		  {
		     int over;

		     count--;
		     over = index + pos + count;
		     base->bitmap[over >> 5] &= ~(1 << (over & 0x1F));
		  }

		if (!first)
		  base->jump = (index + pos + count) >> 5;

		return index + pos;
	     }

	   /* Remove all the tested bit from bitmap. */
	   mask = ~((1 << jump) - 1);
	   map &= mask;
	   index = i << 5;

	   /* Only move jump only on first loop iteration. */
	   first = 1;
	}
      else
	{
	   while (i < (base->count >> 5) && base->bitmap[i] == 0)
	     ++i;
	}
   } while (i < (base->count >> 5));

   /* We should always find a solution as we always put at least
      count + 1 items at the end of the pool. */
   abort();
   return -1;
}

/**
 * Give the pointer corresponding to an item index.
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       index   Item index given by ememoa_memory_base_resize_list_new_item.
 * @return	Will return a pointer to the item.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
void*
ememoa_memory_base_resize_list_get_item (struct ememoa_memory_base_resize_list_s *base, int index)
{
   EMEMOA_CHECK_MAGIC(base);

   if (index < 0)
     return NULL;

   return (void*) ((uint8_t*) base->pool + index * base->size);
}

/**
 * Give back an item to the list.
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       index   Item index given by ememoa_memory_base_resize_list_new_item.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
void
ememoa_memory_base_resize_list_back (struct ememoa_memory_base_resize_list_s *base, int index)
{
   unsigned int                 shift;
   unsigned int                 i;

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

/**
 * Give back items to the list.
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       index   Item index given by ememoa_memory_base_resize_list_new_items.
 * @param       count   How much items to give back.
 * @ingroup	Ememoa_Mempool_Base_Resize_List
 */
void
ememoa_memory_base_resize_list_back_many (struct ememoa_memory_base_resize_list_s *base, int index, int count)
{
   unsigned int                 shift;
   unsigned int                 i;

   EMEMOA_CHECK_MAGIC(base);

   if (index < 0)
     return ;

   /* FIXME: Later improve this, but it will ok for the time being. */
   while (count)
     {
	shift = index >> 5;
	i = index & 0x1F;

	base->bitmap[shift] |= (1 << i);
	base->actif--;

	if (shift < base->jump)
	  base->jump = shift;

#ifdef DEBUG
	memset ((uint8_t*) base->pool + base->size * index, 44, base->size);
#endif

	index++;
	count--;
     }
}

/**
 * Make some attempt to resize the size of the list.
 *
 * @param       base    Pointer to a valid and activ list.
 * @return      Will 0 is nothing where freed, -1 if not enought memory
 *              is available for the operation and anything else if successfull.
 * @ingroup     Ememoa_Mempool_Base_Resize_List
 */
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

/**
 * Call fct on all allocated item of the list and return the sum of fct result.
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       start   Index to start at.
 * @param       end     Index to end the walk.
 * @param       fct     The callback function.
 * @param       ctx     An obscure pointer that will be directly passed, without any
 *                      check/change to each fct call.
 * @return      Will return the sum of fct result.
 * @ingroup     Ememoa_Mempool_Base_Resize_List
 */
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

/**
 * Call fct as long as fct doesn't return a value different from 0.
 *
 * @param       base    Pointer to a valid and activ list.
 * @param       start   Index to start at.
 * @param       end     Index to end the walk.
 * @param       fct     The callback function.
 * @param       ctx     An obscure pointer that will be directly passed, without any
 *                      check/change to each fct call.
 * @param       index   If different from NULL, put the index on which we stop in it.
 * @return      Will return a pointer to the item where we stop the search.
 * @ingroup     Ememoa_Mempool_Base_Resize_List
 */
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

