/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
** This file provide a fast allocator for fixed sized struct
** and will improve data locality
*/

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>

#ifdef	HAVE_PTHREAD
#include <pthread.h>

#define	EMEMOA_LOCK(Memory) \
	if ((Memory->options & EMEMOA_THREAD_PROTECTION) == EMEMOA_THREAD_PROTECTION) \
		pthread_mutex_lock(&(Memory->lock));

#define	EMEMOA_UNLOCK(Memory) \
	if ((Memory->options & EMEMOA_THREAD_PROTECTION) == EMEMOA_THREAD_PROTECTION) \
		pthread_mutex_unlock(&(Memory->lock));

#else

#define EMEMOA_LOCK(Memory)	;
#define EMEMOA_UNLOCK(Memory)	;

#endif

#include "ememoa_mempool_fixed.h"
#include "ememoa_memory_base.h"
#include "mempool_struct.h"

#define	EMEMOA_MAGIC	0x4224007

#ifdef DEBUG
#define	EMEMOA_CHECK_MAGIC(Memory) \
	assert(Memory->magic == EMEMOA_MAGIC); \
	assert(Memory->max_objects_pot >= BITMASK_POWER);
#else
#define	EMEMOA_CHECK_MAGIC(Memory) ;
#endif

#if SIZEOF_VOID == 8
typedef uint64_t	bitmask_t;
# define BITMASK_POWER	6

#else
typedef uint32_t	bitmask_t;
# define BITMASK_POWER	5

#endif

/* Macro for general alignment */
#define EMEMOA_ALIGN(Ptr) \
  {\
    Ptr--;\
    Ptr |= sizeof (void*) - 1;\
    Ptr++;\
  };

/* WARNING: THIS CAST MUST BE VALIDATED ON ALL ARCHITECTURE */
#define EMEMOA_EVAL_BOUND_LOW(Pointer) ((unsigned long) Pointer)
#define EMEMOA_EVAL_BOUND_HIGH(Pointer, Memory) (((unsigned long) Pointer) + Memory.object_size * Memory.max_objects)

#define	EMEMOA_INDEX_LOW(Index) (Index & ((1 << BITMASK_POWER) - 1))
#define EMEMOA_INDEX_HIGH(Index) (Index >> BITMASK_POWER)

#define EMEMOA_SIZEOF_POOL(Memory) (sizeof (uint8_t) * Memory->object_size * Memory->max_objects)

#ifdef	ememoa_mempool_fixed_display_statistic
#undef	ememoa_mempool_fixed_display_statistic
#endif

#ifdef	ememoa_mempool_fixed_display_statistic_all
#undef	ememoa_mempool_fixed_display_statistic_all
#endif

struct ememoa_mempool_fixed_pool_s
{
   unsigned int         jump_object;
   unsigned int         available_objects;
   bitmask_t            *objects_use;
   void                 *objects_pool;
};

struct ememoa_memory_base_resize_list_s *fixed_pool_list = NULL;

/**
 * @defgroup Ememoa_Mempool_Fixed Memory pool manipulation functions for fixed size object
 *
 */

/**
 * Allocate a new mempool structur.
 *
 * @return	Will return the index of the mempool or -1 if it failed.
 * @ingroup	Ememoa_Search_Mempool
 */
static int
new_ememoa_fixed_pool ()
{
   if (!fixed_pool_list)
     fixed_pool_list = ememoa_memory_base_resize_list_new (sizeof (struct ememoa_mempool_fixed_s));

   if (fixed_pool_list == NULL)
     return -1;

   return ememoa_memory_base_resize_list_new_item (fixed_pool_list);
}

/**
 * Search the mempool structur matching the specified index.
 *
 * @param       index   The memory pool index you want to retrieve.
 * @return              Will return a pointer to the mempool if succeeded and NULL otherwise.
 * @ingroup     Ememoa_Search_Mempool
 */
struct ememoa_mempool_fixed_s*
ememoa_mempool_fixed_get_index (unsigned int index)
{
   if (fixed_pool_list == NULL)
     return NULL;

   return ememoa_memory_base_resize_list_get_item (fixed_pool_list, index);
}

/**
 * Give back a mempool index. This mempool index will be given back during a later call new_ememoa_fixed_pool().
 *
 * @param       index   The memory pool index, you want to give back.
 * @ingroup     Ememoa_Search_Mempool
 */
static void
ememoa_mempool_fixed_back (unsigned int index)
{
   if (fixed_pool_list)
     ememoa_memory_base_resize_list_back (fixed_pool_list, index);
}

/**
 * Initializes a memory pool structure for later use
 *
 * The following example code demonstrates how to ensure that the
 * given memory pool has been successfully initialized.
 *
 * @code
 * int	test_pool = ememoa_mempool_fixed_init (sizeof (int), 10, NULL);
 * if (test_pool < 0)
 * {
 *    fprintf (stderr, "ERROR: Memory pool initialization failed\n");
 *    exit (-1);
 * }
 * @endcode
 *
 * @param	object_size		Size of the object that will be provided by
 *					the new pool. If @c 0 is passed, the call will
 *					fail.
 * @param	preallocated_item_pot	Number of items tobe  pre-allocated by the pool. They
 *					are exprimed in power of two (10 will mean
 *					1024 elements per pool). If @c 0 is passed the
 *					call will fail. Using 1 will really be
 *					stupid.
 * @param	options			This parameter will give you the possibility to take
 *					into account the exact pattern usage of the memory pool.
 *					Right now, only EMEMOA_THREAD_PROTECTION is supported,
 *					but futur improvement could use this facilty too.
 * @param	desc			Pointer to a valid description for this new pool.
 *					If @c NULL is passed, you will not be able to
 *					see the content of the memory for debug purpose.
 * @return	Will return @c -1 if the initialization of the memory pool failed, and the index
 *		of the memory pool if it succeed.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_init (unsigned int				object_size,
                           unsigned int				preallocated_item_pot,
                           unsigned int				options,
                           const struct ememoa_mempool_desc_s	*desc)
{
   int					index = new_ememoa_fixed_pool ();
   struct ememoa_mempool_fixed_s	*memory = ememoa_mempool_fixed_get_index(index);

   if (index == -1)
     return -1;

   if (memory == NULL)
     return -1;

   assert(object_size > 0);
   assert(preallocated_item_pot > 0);

   bzero (memory, sizeof (struct ememoa_mempool_fixed_s));

   EMEMOA_ALIGN(object_size);
   memory->object_size = object_size;
   /* First make an upper approximation of the minimal
      number of objects per pool that fit in n * bitmask_t */
   memory->max_objects_pot = preallocated_item_pot < BITMASK_POWER ? BITMASK_POWER : preallocated_item_pot;
   memory->max_objects_poi = (1 << (memory->max_objects_pot - BITMASK_POWER));
   memory->max_objects = (1 << memory->max_objects_pot);

#ifdef DEBUG
   memory->max_out_objects = 0;
   memory->out_objects = 0;
   memory->magic = EMEMOA_MAGIC;
#endif

   memory->desc = desc;
   memory->last_error_code = EMEMOA_NO_ERROR;

   memory->base = ememoa_memory_base_resize_list_new (sizeof (struct ememoa_mempool_fixed_pool_s));
   memory->jump_pool = 0;
   memory->options = options;

#ifdef HAVE_PTHREAD
   pthread_mutex_init (&(memory->lock), NULL);
#endif

   return index;
}

/**
 * Destroys all allocated objects of the memory pool and uninitialize them. The
 * memory pool is unusable after the call of this function.
 *
 * The following example code demonstrates how to ensure that
 * a given memory pool has been successfully destroyed
 *
 * @code
 * if (ememoa_mempool_fixed_clean (test_pool))
 * {
 *    fprintf (stderr, "ERROR: Memory pool destruction failed.\n");
 *    exit (-1);
 * }
 * @endcode
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @return	Will return @c 0 if successfully cleaned.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_clean (int	mempool)
{
   struct ememoa_mempool_fixed_s        *memory = ememoa_mempool_fixed_get_index (mempool);
   int                                  error_code = 0;

   error_code = ememoa_mempool_fixed_free_all_objects (mempool);
   if (error_code)
     return error_code;

#ifdef HAVE_PTHREAD
   pthread_mutex_destroy (&(memory->lock));
#endif

   ememoa_memory_base_resize_list_clean (memory->base);
   ememoa_mempool_fixed_back (mempool);
   bzero (memory, sizeof (struct ememoa_mempool_fixed_s));

   return 0;
}

/**
 * @defgroup Ememoa_Alloc_Mempool Helper function for object allocation
 *
 */

/**
 * Set the bit corresponding to the allocated address as used.
 *
 * @param	index_l			Lower part of the index (inside one bitmask_t)
 * @param	index_h			Higher part of the index (index in the bitmask_t[])
 * @param	objects_use_slot	Pointer to the bitmask_t lookup table.
 * @param	jump_object_slot	Fast jump to the first available entry in the table.
 * @ingroup	Ememoa_Alloc_Mempool
 */
static void
set_address (unsigned int	index_l,
	    unsigned int	index_h,
	    bitmask_t		*objects_use_slot,
	    unsigned int	*jump_object_slot)
{
   bitmask_t	mask = 1;

   assert(objects_use_slot != NULL);

   mask <<= index_l;

   if ((objects_use_slot[index_h] &= ~mask) == 0)
     *jump_object_slot += (*jump_object_slot == index_h) ? 1 : 0;
}

/**
 * Allocate a new empty pool inside a Mempool
 *
 * @param	memory		Pointer to a valid address of a memory pool. If
 *				an invalid pool is passed, bad things will happen.
 * @return	Will return @c -1 if an error occurred or the index pool otherwise.
 * @ingroup	Ememoa_Alloc_Mempool
 */
static struct ememoa_mempool_fixed_pool_s*
add_pool (struct ememoa_mempool_fixed_s *memory)
{
   struct ememoa_mempool_fixed_pool_s   *pool;
   int                                  index;

   index = ememoa_memory_base_resize_list_new_item (memory->base);
   pool = ememoa_memory_base_resize_list_get_item (memory->base, index);

   if (pool == NULL)
     return NULL;

   pool->objects_use = ememoa_memory_base_alloc (sizeof (bitmask_t) * memory->max_objects_poi);
   pool->objects_pool = ememoa_memory_base_alloc (EMEMOA_SIZEOF_POOL(memory));
   pool->available_objects = memory->max_objects - 1;
   pool->jump_object = 0;

   if (pool->objects_use == NULL
       || pool->objects_pool == NULL)
     {
	ememoa_memory_base_free (pool->objects_use);
	ememoa_memory_base_free (pool->objects_pool);
        ememoa_memory_base_resize_list_back (memory->base, index);
	memory->last_error_code = EMEMOA_ERROR_MALLOC_NEW_POOL;

	return NULL;
     }

#ifdef DEBUG
   memset (pool->objects_pool, 42, EMEMOA_SIZEOF_POOL(memory));
#endif

   /* Set all objects as available */
   memset (pool->objects_use, 0xFF, sizeof (bitmask_t) * memory->max_objects_poi);

   return pool;
}

/**
 * This callback is used to find a pool with available slot for new object. It is used during
 * the allocation with of a new object.
 *
 * @param       ctx     Useless in this context.
 * @param       index   Useless in this context.
 * @param       data    Pointer to a pool to test.
 * @return      Will return TRUE if some space is available, FALSE otherwise.
 * @ingroup     Ememoa_Mempool_Fixed
 */
static int
ememoa_mempool_fixed_lookup_empty_pool_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_pool_s   *pool = data;

   (void) index; (void) ctx;

   return pool->available_objects > 0;
}

/**
 * Pops a new object out of the memory pool
 *
 * The following example code demonstrate how to ensure that a
 * pointer has been successfully retrieved from the memory pool.
 *
 * @code
 *   object_s *new_object = ememoa_mempool_fixed_pop_object (mempool_of_object);
 *
 *   if (new_object == NULL)
 *   {
 *	fprintf (stderr, "ERROR: %s", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (mempool_of_object)));
 *	exit (-1);
 *   }
 * @endcode
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @return	Will return @c NULL if it was impossible to allocate any data. Check
 *		memory->last_error_code and ememoa_mempool_error2string to know why.
 * @ingroup	Ememoa_Mempool_Fixed
 */
void*
ememoa_mempool_fixed_pop_object (int mempool)
{
   struct ememoa_mempool_fixed_s        *memory = ememoa_mempool_fixed_get_index (mempool);
   struct ememoa_mempool_fixed_pool_s   *pool;
   uint8_t				*start_address = NULL;

   EMEMOA_CHECK_MAGIC(memory);
   EMEMOA_LOCK(memory);

   pool = ememoa_memory_base_resize_list_search_over (memory->base,
                                                      memory->jump_pool,
                                                      -1,
                                                      ememoa_mempool_fixed_lookup_empty_pool_cb,
                                                      NULL,
                                                      &memory->jump_pool);

   if (pool != NULL)
     {
	bitmask_t	*itr = pool->objects_use;
	bitmask_t	reg;
	int		index = 0;

        /*
	  If bit is set to 0, then the corresponding memory object is in use
	  if bit is set to 1, then the corresponding memory object is available
	*/
	for (itr += pool->jump_object;
	     *itr == 0;
	     ++pool->jump_object, ++itr)
	  ;

	index = pool->jump_object << BITMASK_POWER;
	reg = *itr;

#ifdef USE64
	index += ffsll (reg) - 1;
#else
	index += ffs (reg) - 1;
#endif

	/* Remove available objects from the slot and update jump_pool if necessary */
	if (--pool->available_objects == 0)
	  memory->jump_pool++;

	start_address = pool->objects_pool + index * memory->object_size;

	set_address (EMEMOA_INDEX_LOW(index),
                    EMEMOA_INDEX_HIGH(index),
                    pool->objects_use,
                    &pool->jump_object);
     }
   else
     {
        pool = add_pool (memory);
	if (pool != NULL)
	  {
	     start_address = pool->objects_pool;
	     set_address (0, 0,
                         pool->objects_use,
                         &pool->jump_object);
	  }
	else
	  memory->last_error_code = EMEMOA_NO_MORE_MEMORY;
     }

#ifdef DEBUG
   memory->max_out_objects += (memory->max_out_objects < ++memory->out_objects) ? 1 : 0;
#endif

   EMEMOA_UNLOCK(memory);
   return start_address;
}

/**
 * Callback destroying all the content of the memory pool.
 *
 * @param       ctx     Useless in this context.
 * @param       index   Useless in this context.
 * @param       data    Pointer to the pool to be cleaned.
 * @return      Will return @c 1 if successfull.
 * @ingroup     Ememoa_Mempool_Fixed
 */
static int
ememoa_mempool_fixed_free_pool_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_pool_s   *pool = data;

   (void) ctx; (void) index;

   ememoa_memory_base_free (pool->objects_use);
   ememoa_memory_base_free (pool->objects_pool);

   return 1;
}

/**
 * Destroys all allocated object of the memory pool. The memory pool is still
 * usable after the call of this function.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @return	Will return @c 0 if successfully cleaned.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_free_all_objects (int mempool)
{
   struct ememoa_mempool_fixed_s        *memory = ememoa_mempool_fixed_get_index (mempool);

   EMEMOA_CHECK_MAGIC(memory);
   EMEMOA_LOCK(memory);

   ememoa_memory_base_resize_list_walk_over (memory->base, 0, -1, ememoa_mempool_fixed_free_pool_cb, NULL);
   ememoa_memory_base_resize_list_clean (memory->base);

#ifdef DEBUG
   memory->out_objects = 0;
#endif

   memory->base = ememoa_memory_base_resize_list_new (sizeof (struct ememoa_mempool_fixed_pool_s));

   EMEMOA_UNLOCK(memory);

   return 0;
}

struct ememoa_mempool_fixed_push_ctx_s
{
   void                                 *ptr;
   uint8_t                              *ptr_inf;
   struct ememoa_mempool_fixed_s        *memory;
};

/**
 * Callback checking and pushing back in a previously allocated chunk.
 *
 * @param       ctx     Push context (precomputed value checked against each pool).
 * @param       index   Useless in this context.
 * @param       data    Pointer to the pool to check.
 * @return      Will return @c 1 if successfull.
 * @ingroup     Ememoa_Mempool_Fixed
 */
static int
ememoa_mempool_fixed_push_object_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_push_ctx_s       *pctx = ctx;
   struct ememoa_mempool_fixed_pool_s           *pool = data;

   (void) index;

   if (pool->objects_pool <= pctx->ptr && pctx->ptr_inf <= ((uint8_t*) pool->objects_pool))
     {
        bitmask_t		*objects_use = pool->objects_use;
        bitmask_t		mask = 1;
        /*
          High risk, if one day sizeof (unsigned long) > sizeof (void*)
          something will go wrong...
        */
        unsigned long	position = ((uint8_t*) pctx->ptr - (uint8_t*) pool->objects_pool) / pctx->memory->object_size;
        unsigned int	index_h = EMEMOA_INDEX_HIGH(position);
        unsigned int	index_l = EMEMOA_INDEX_LOW(position);

        mask <<= index_l;

        EMEMOA_LOCK(pctx->memory);

#ifdef DEBUG
        pctx->memory->out_objects--;
#endif
        if (objects_use[index_h] & mask)
          {
             pctx->memory->last_error_code = EMEMOA_DOUBLE_PUSH;

             EMEMOA_UNLOCK(pctx->memory);
             return 1;
          }

        objects_use[index_h] |= mask;
        pool->available_objects++;

        if (pool->jump_object > index_h)
          pool->jump_object = index_h;

        if (pctx->memory->jump_pool > index)
          pctx->memory->jump_pool = index;

        EMEMOA_UNLOCK(pctx->memory);
        return 1;
     }
   return 0;
}

/**
 * Push back an object in the memory pool
 *
 * The following example code demonstrates how to ensure that a
 * given pointer has been successfully given back to his memory pool.
 *
 * @code
 *   if (ememoa_mempool_fixed_push_object (mempool_of_object, new_object))
 *   {
 *	fprintf (stderr, "ERROR: %s", ememoa_mempool_error2string ( ememoa_mempool_fixed_get_last_error (mempool_of_object)));
 *	exit (-1);
 *   }
 * @endcode
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @param	ptr		Pointer to object that belongs to @c memory mempool.
 * @return	Will return @c 0 if it was successfully pushed back to the memory pool. If not, check
 *		memory->last_error_code and ememoa_mempool_error2string to know why.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_push_object (int	mempool,
                                  void	*ptr)
{
   struct ememoa_mempool_fixed_s                *memory = ememoa_mempool_fixed_get_index(mempool);
   struct ememoa_mempool_fixed_pool_s           *pool;
   struct ememoa_mempool_fixed_push_ctx_s       pctx;
   unsigned int                                 size;

   EMEMOA_CHECK_MAGIC(memory);

   size = memory->object_size * memory->max_objects;

   pctx.ptr = ptr;
   pctx.ptr_inf = ((uint8_t*) ptr) - size;
   pctx.memory = memory;

   pool = ememoa_memory_base_resize_list_search_over (memory->base,
                                                      0,
                                                      -1,
                                                      ememoa_mempool_fixed_push_object_cb,
                                                      &pctx,
                                                      NULL);

   if (pool)
     return 0;

   memory->last_error_code = EMEMOA_ERROR_PUSH_ADDRESS_NOT_FOUND;
   return -1;
}

/**
 * Callback freeing empty pool.
 *
 * @param       ctx     Pointer to the current memory pool.
 * @param       index   Useless in this context.
 * @param       data    Pointer to the pool to freed.
 * @return      Will return @c 0 if freed.
 * @ingroup     Ememoa_Alloc_Mempool
 */
static int
ememoa_used_pool_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_s        *memory = ctx;
   struct ememoa_mempool_fixed_pool_s   *pool = data;

   (void) index;

   if (pool->available_objects != memory->max_objects)
     return 1;

   ememoa_memory_base_free (pool->objects_use);
   ememoa_memory_base_free (pool->objects_pool);

   pool->objects_use = NULL;
   pool->objects_pool = NULL;
   pool->available_objects = 0;

   ememoa_memory_base_resize_list_back (memory->base, index);
   return 0;
}

/**
 * Walks on each allocated pool and free it, if empty.
 *
 * @param	memory		Pointer to a valid address of a memory pool. If
 *				an invalid pool is passed, bad things will happen.
 * @return	Will @c count the number of used pools and give the information back.
 * @ingroup	Ememoa_Alloc_Mempool
 */
static int
ememoa_mempool_fixed_garbage_collect_struct (struct ememoa_mempool_fixed_s *memory)
{
   unsigned int				allocated_pool = 0;

   EMEMOA_CHECK_MAGIC(memory);

   if (memory->base->count == 0)
     return 0;

   EMEMOA_LOCK(memory);

   allocated_pool = ememoa_memory_base_resize_list_walk_over (memory->base,
                                                              0,
                                                              -1,
                                                              ememoa_used_pool_cb,
                                                              memory);

   if (allocated_pool == memory->base->count)
     {
	EMEMOA_UNLOCK(memory);

	memory->last_error_code = EMEMOA_NO_EMPTY_POOL;
	return -1;
     }

   if (allocated_pool != 0)
     {
        ememoa_memory_base_resize_list_garbage_collect (memory->base);

        return 0;
     }

   ememoa_memory_base_resize_list_clean (memory->base);
   memory->base = ememoa_memory_base_resize_list_new (sizeof (struct ememoa_mempool_fixed_pool_s));

   EMEMOA_UNLOCK(memory);

   return 0;
}

/**
 * Collects all the empty pool and resizes the Mempool accordingly.
 *
 * The following example code demonstrates how to ensure that a
 * given Mempool gives back some memory or die.
 *
 * @code
 *   if (ememoa_mempool_fixed_garbage_collect (mempool_of_object))
 *   {
 *	fprintf (stderr, "ERROR: %s", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (mempool_of_object)));
 *	exit (-1);
 *   }
 * @endcode
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @return	Will return @c 0 if some pools were freed.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_garbage_collect(int mempool)
{
   struct ememoa_mempool_fixed_s        *memory = ememoa_mempool_fixed_get_index(mempool);

   return ememoa_mempool_fixed_garbage_collect_struct (memory);
}

/**
 * Callback running garbage collector on a memory pool.
 *
 * @param       ctx     Useless in this context.
 * @param       index   Useless in this context.
 * @param       data    Pointer to a memory pool.
 * @return      Will return @c 0 if some pool where freed.
 * @ingroup     Ememoa_Alloc_Mempool
 */
static int
ememoa_memory_base_walk_over_gc_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_s        *memory = data;
   (void) index; (void) ctx;

   return ememoa_mempool_fixed_garbage_collect_struct (memory);
}

/**
 * Call the garbage collector on all allocated memory pool and reclaim memory as much
 * as possible.
 *
 * @code
 *   if (ememoa_mempool_fixed_garbage_collect_all () == 0)
 *   {
 *	fprintf (stderr, "ERROR: Every bit of memory are in use.");
 *	exit (-1);
 *   }
 * @endcode
 *
 * @return	Will return @c 0 if some pools were freed.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_garbage_collect_all (void)
{
   return ememoa_memory_base_resize_list_walk_over (fixed_pool_list, 0, -1, ememoa_memory_base_walk_over_gc_cb, NULL);
}

struct ememoa_mempool_fixed_walk_ctx_s
{
   struct ememoa_mempool_fixed_s        *memory;
   ememoa_fctl                          fctl;
   void                                 *data;
   int                                  error;
};

/**
 * Callback running fctl over all allocated data in a pool.
 *
 * @param       ctx     Pointer to usefull context data.
 * @param       index   Useless in this context.
 * @param       data    Pointer to a memory pool.
 * @return      Will return @c 0 if every thing run correctly.
 * @ingroup     Ememoa_Mempool_Fixed
 */
static int
ememoa_mempool_fixed_walk_over_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_pool_s           *pool = data;
   struct ememoa_mempool_fixed_walk_ctx_s       *wctx = ctx;
   uint8_t                                      *start_address = pool->objects_pool;
   bitmask_t                                    *objects_use = pool->objects_use;
   unsigned int                                 j, k;

   (void) index;

   for (j = 0; j < wctx->memory->max_objects_poi; ++j)
     {
        bitmask_t	value = *objects_use++;

        for (k = 0;
             k < (1 << BITMASK_POWER);
             ++k, value >>= 1, start_address += wctx->memory->object_size)
          if ((value & 1) == 0)
            {
               wctx->error = wctx->fctl (start_address, wctx->data);
               if (wctx->error)
                 return -1;
            }
     }
   return 0;
}

/**
 * Executes fctl on all allocated data in the pool. If the execution of fctl returns something
 * else than 0, then the walk ends and returns the error code provided by fctl.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @param	fctl		Function pointer that must be run on all allocated
 *				objects.
 * @param	data		Pointer that will be passed as is to each call to fctl.
 * @return	Will return @c 0 if the run walked over all allocated objects.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_walk_over (int		mempool,
				ememoa_fctl	fctl,
				void		*data)
{
   struct ememoa_mempool_fixed_s                *memory = ememoa_mempool_fixed_get_index (mempool);
   struct ememoa_mempool_fixed_walk_ctx_s       wctx;

   EMEMOA_CHECK_MAGIC(memory);
   EMEMOA_LOCK(memory);

   wctx.memory = memory;
   wctx.data = data;
   wctx.fctl = fctl;

   ememoa_memory_base_resize_list_search_over (memory->base,
                                               0,
                                               -1,
                                               ememoa_mempool_fixed_walk_over_cb,
                                               &wctx,
                                               NULL);

   EMEMOA_UNLOCK(memory);
   return wctx.error;
}

/**
 * @defgroup Ememoa_Display_Mempool Function displaying statistic usefull during debug
 *
 */

/**
 * Displays the bitmask corresponding to the usage of the pool. '.' means in use, '|' means empty.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @param	index		Pool's index in the mempool.
 * @ingroup	Ememoa_Display_Mempool
 */
static int
ememoa_mempool_fixed_display_pool_cb (void *ctx, int index, void *data)
{
   struct ememoa_mempool_fixed_s        *memory = ctx;
   struct ememoa_mempool_fixed_pool_s   *pool = data;
   unsigned int				i, j;
   char					display[64] = "";
   bitmask_t				value;
   unsigned long			bound_l, bound_h;

   (void) index;

   bound_l = EMEMOA_EVAL_BOUND_LOW(pool->objects_pool);
   bound_h = EMEMOA_EVAL_BOUND_HIGH(pool->objects_pool, (*memory));

   printf ("Available objects: %i\n", pool->available_objects);
   printf ("Start at: %p, end at: %p\n", (void*) bound_l, (void*) bound_h);

   if (bound_l != 0)
     {
	display[(1 << BITMASK_POWER)] = '\0';
	for (i = 0; i < memory->max_objects_poi; ++i)
	  {
	     bitmask_t	*objects_use = pool->objects_use;

	     value = objects_use[i];
	     for (j = 0; j < (1 << BITMASK_POWER); ++j, value >>= 1)
	       /*
		 if bit is set to 0, then the corresponding memory object is in use
		 if bit is set to 1, then the corresponding memory object is available
	       */
	       display[j] = (value & 1) ? '.' : '|';

	     if (i & 1)
	       printf ("%s\n", display);
	     else
	       printf ("%s", display);
	  }
	if (i & 1)
	  printf ("\n");
     }

   return 0;
}

/**
 * Displays all the statistic currently known about a Mempool, usefull to dimension it.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @ingroup	Ememoa_Display_Mempool
 */
void
ememoa_mempool_fixed_display_statistic (int mempool)
{
   struct ememoa_mempool_fixed_s        *memory = ememoa_mempool_fixed_get_index (mempool);

   printf ("Memory information for pool located at : %p\n", (void*) memory);

   EMEMOA_LOCK(memory);

#ifdef	DEBUG
   printf ("Memory magic is : %x\n", memory->magic);
   if (memory->magic != EMEMOA_MAGIC)
     return ;
#endif

   if (memory->desc && memory->desc->name)
     printf ("This pool contains : %s.\n", memory->desc->name);

   printf ("Objects per pool: %i[%x] (for poi: %i, pot: %i)\n", memory->max_objects, memory->max_objects, memory->max_objects_poi, memory->max_objects_pot);
   printf ("Object size: %i\n", memory->object_size);
   printf ("Allocated pool: %i\n", memory->base->actif);

#ifdef DEBUG
   printf ("Objects currently delivered: %i.\n", memory->out_objects);
   printf ("Total objects currently in pool: %i.\n", memory->max_objects * memory->base->actif);
   printf ("Maximum delivered objects since the birth of the memory pool: %i.\n", memory->max_out_objects);
#endif

   ememoa_memory_base_resize_list_walk_over (memory->base,
                                             0,
                                             -1,
                                             ememoa_mempool_fixed_display_pool_cb,
                                             memory);

   if (memory->desc)
     {
	char*	name = alloca (strlen(memory->desc->name ? memory->desc->name : ""));
	strcpy (name, memory->desc->name);

	if (memory->desc->data_display)
	  {
	     printf ("=== Content ===\n");
	     ememoa_mempool_fixed_walk_over (mempool, memory->desc->data_display, name);
	     printf ("=== ===\n");
	  }
     }

   EMEMOA_UNLOCK(memory);
}

/**
 * Callback used to count active pool.
 * FIXME: It must exist a better way to do it.
 *
 * @param       ctx     Useless in this context.
 * @param       index   Useless in this context.
 * @param       data    Useless in this context.
 * @return      Will return @c 1 if every thing run correctly.
 * @ingroup     Ememoa_Display_Mempool
 */
static int
ememoa_mempool_fixed_display_statistic_count_cb (void* ctx, int index, void *data)
{
   (void) ctx; (void) index; (void) data;

   return 1;
}

/**
 * Callback for displaying statistic of a memory pool.
 *
 * @param       ctx     Useless in this context.
 * @param       index   Memory pool index.
 * @param       data    Useless in this context.
 * @return      Will return @c 0 if every thing run correctly.
 * @ingroup     Ememoa_Display_Mempool
 */
static int
ememoa_mempool_fixed_display_statistic_cb (void* ctx, int index, void *data)
{
   (void) ctx; (void) data;

   ememoa_mempool_fixed_display_statistic (index);
   return 0;
}

/**
 * Displays all the statistic currently known about all active Mempool.
 *
 * @ingroup	Ememoa_Display_Mempool
 */
void
ememoa_mempool_fixed_display_statistic_all (void)
{
   int                                  result = 0;

   result = ememoa_memory_base_resize_list_walk_over (fixed_pool_list, 0, -1, ememoa_mempool_fixed_display_statistic_count_cb, NULL);
   printf ("%i mempool are used in %i currently allocated.\n", result, fixed_pool_list->count);
   ememoa_memory_base_resize_list_walk_over (fixed_pool_list, 0, -1, ememoa_mempool_fixed_display_statistic_cb, NULL);
}

