/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#ifdef HAVE_PTHREAD
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
#include "ememoa_mempool_unknown_size.h"
#include "ememoa_mempool_struct.h"
#include "mempool_struct.h"

#define	EMEMOA_MAGIC	0x4224008

#ifdef DEBUG
#define	EMEMOA_CHECK_MAGIC(Memory) \
	assert(Memory != NULL); \
	assert(Memory->magic == EMEMOA_MAGIC);
#else
#define	EMEMOA_CHECK_MAGIC(Memory) ;
#endif

struct ememoa_mempool_unknown_size_item_s;

struct ememoa_mempool_alloc_item_s
{
   struct ememoa_mempool_alloc_item_s		*next;
   struct ememoa_mempool_alloc_item_s		*prev;
   struct ememoa_mempool_unknown_size_item_s	*data;
};

struct ememoa_mempool_unknown_size_item_s
{
   int					index;
#ifdef DEBUG
   unsigned int				magic;
#endif
   struct ememoa_mempool_alloc_item_s	*item;
   void*					data;
};

struct ememoa_mempool_unknown_size_s	*all_unknown_size_pool = NULL;
static unsigned int			all_unknown_size_pool_count = 0;

static unsigned int				new_ememoa_unknown_pool ()
{
   struct ememoa_mempool_unknown_size_s		*temp;
   unsigned int					i;

   for (i = 0; i < all_unknown_size_pool_count; ++i)
     if (all_unknown_size_pool[i].in_use == 0)
       return i;

   ++all_unknown_size_pool_count;
   temp = realloc (all_unknown_size_pool, sizeof (struct ememoa_mempool_unknown_size_s) * all_unknown_size_pool_count);

   assert (temp != NULL);

   all_unknown_size_pool = temp;

   return all_unknown_size_pool_count - 1;
}

/**
 * @defgroup Ememoa_Mempool_Unknown_Size Memory pool manipulation functions for variable size objects
 *
 */

static int
ememoa_mempool_unknown_size_partial_clean (struct ememoa_mempool_unknown_size_s	*memory,
					   unsigned int				position,
					   ememoa_mempool_error_t		error_code)
{
   unsigned int	i = position;

   for (--i; i > 0; --i)
     ememoa_mempool_fixed_clean (memory->pools[i]);

   free (memory->pools_match);
   free (memory->pools);

   bzero (memory, sizeof (struct ememoa_mempool_unknown_size_s));
   memory->last_error_code = error_code;

   return -1;
}

/**
 * Initializes a memory pool structure for later use
 * 
 * The following example code demonstrates how to ensure that the
 * given memory pool has been successfully initialized.
 *
 * @code
 * struct ememoa_mempool_unknown_size_s	test_pool;
 *
 * if (ememoa_mempool_unknown_size_init (&test_pool, sizeof(default_map_size_count)/(sizeof(unsigned int) * 2), default_map_size_count, NULL))
 * {
 *    fprintf (stderr, "ERROR: Memory pool initialization failed\n");
 *    exit (-1);
 * }
 * @endcode
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c NULL is passed, the call will fail. If the
 *					address was already an initialized memory pool,
 *					bad things will happen to your program.
 * @param	map_items_count		Number of fixed size pools to create.
 * @param	map_size_count		Array of element describing fixed pools used by
 *					this memory pool. Odd row for size, even row
 *					for preallocated items. Take a look at
 *					default_map_size_count.
 * @param	options			This parameter will give you the possibility to take
 *					into account the exact pattern usage of the memory pool.
 *					Right now, only EMEMOA_THREAD_PROTECTION is supported,
 *					but futur improvement could use this facilty too.
 * @param	desc			Pointer to a valid description for this new pool.
 *					If @c NULL is passed, you will not be able to
 *					see the contents of the memory for debug purpose.
 * @return	Will return @c 0 if succeed in the initialization of the memory pool.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
unsigned int
ememoa_mempool_unknown_size_init (unsigned int				map_items_count,
				  const unsigned int			*map_size_count,
				  unsigned int				options,
				  const struct ememoa_mempool_desc_s	*desc)
{
   unsigned int				index = new_ememoa_unknown_pool ();
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + index;

   unsigned int	i;

   assert (memory != NULL);
   assert (map_items_count > 0);
   assert (map_size_count != NULL);

   bzero (memory, sizeof (struct ememoa_mempool_unknown_size_s));

#ifdef DEBUG
   memory->magic = EMEMOA_MAGIC;
#endif

   memory->pools_count = map_items_count;
   memory->pools_match = malloc (sizeof (unsigned int) * map_items_count);
   memory->pools = malloc (sizeof (unsigned int) * map_items_count);

   for (i = 0; i < map_items_count; ++i)
     {
	memory->pools_match[i] = map_size_count[(i << 1) + 0];
	memory->pools[i] = ememoa_mempool_fixed_init (map_size_count[(i << 1) + 0] + sizeof (struct ememoa_mempool_unknown_size_item_s),
						      map_size_count[(i << 1) + 1],
						      options,
						      NULL);
     }

   memory->allocated_list = ememoa_mempool_fixed_init (sizeof (struct ememoa_mempool_alloc_item_s),
						       7,
						       options,
						       NULL);

   memory->start = NULL;
   memory->desc = desc;
   memory->last_error_code = EMEMOA_NO_ERROR;

   memory->options = options;
   memory->in_use = 1;

#ifdef HAVE_PTHREAD
   pthread_mutex_init (&(memory->lock), NULL);
#endif

   return index;
}

/**
 * Destroys all allocated objects of the memory pool and uninitialize it. The 
 * memory pool is unusable after the call of the function.
 *
 * The following example code demonstrate how to ensure that
 * a given memory pool has been successfully destroyed
 *
 * @code
 * if (ememoa_mempool_unknown_size_clean (&test_pool))
 * {
 *    fprintf (stderr, "ERROR: Memory pool destruction failed.\n");
 *    exit (-1);
 * }
 * @endcode
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c NULL is passed, the call will fail. If the
 *					address was already a initialized memory pool,
 *					bad things will happen to your program.
 * @return	Will return @c 0 if successfully cleaned.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
int
ememoa_mempool_unknown_size_clean (unsigned int		mempool)
{
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;

   EMEMOA_CHECK_MAGIC(memory);

   ememoa_mempool_unknown_size_free_all_objects (mempool);

#ifdef HAVE_PTHREAD
   pthread_mutex_destroy (&(memory->lock));
#endif

   free (memory->pools_match);
   free (memory->pools);

   bzero (memory, sizeof (struct ememoa_mempool_unknown_size_s));
  
   return 0;
}

/**
 * Destroys all allocated object of the memory pool. The memory pool is still
 * usable after the call of this function.
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c NULL is passed, the call will fail. If the
 *					address was already a initialized memory pool,
 *					bad things will happen to your program.
 * @return	Will return @c 0 if successfully cleaned.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
int
ememoa_mempool_unknown_size_free_all_objects (unsigned int	mempool)
{
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;
   unsigned int				i;

   EMEMOA_CHECK_MAGIC(memory);

   for (i = 0; i < memory->pools_count; ++i)
     if (ememoa_mempool_fixed_free_all_objects (memory->pools[i]))
       {
	  memory->last_error_code = ememoa_mempool_fixed_get_last_error (memory->pools[i]);
	  return -1;
       }

   EMEMOA_LOCK(memory);

   if (ememoa_mempool_fixed_free_all_objects (memory->allocated_list))
     {
	memory->last_error_code = ememoa_mempool_fixed_get_last_error (memory->allocated_list);
	EMEMOA_UNLOCK(memory);
	return -1;
     }

   memory->start = NULL;

   EMEMOA_UNLOCK(memory);
   return 0;  
}

/**
 * Push back an object in the memory pool
 *
 * The following example code demonstrate how to ensure that a
 * given pointer has been successfully given back to his memory pool.
 *
 * @code
 *   if (ememoa_mempool_unknown_size_push_object (&mempool_of_object, new_object))
 *   {
 *	fprintf (stderr, "ERROR: %s", ememoa_mempool_error2string (mempool_of_object.last_error_code));
 *	exit (-1);
 *   }
 * @endcode
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c memory is not a valid memory pool, bad things
 *					will happen.
 * @param	ptr			Pointer to an object belonging to @c memory mempool.
 * @return	Will return @c 0 if it was successfully pushed back to the memory pool. Else, check
 *		memory->last_error_code and ememoa_mempool_error2string to know why.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
int
ememoa_mempool_unknown_size_push_object (unsigned int	mempool,
					 void		*ptr)
{
   struct ememoa_mempool_unknown_size_item_s	*old = (struct ememoa_mempool_unknown_size_item_s*)ptr - 1;
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;

   assert (ptr != NULL);
   EMEMOA_CHECK_MAGIC(memory);
   EMEMOA_CHECK_MAGIC(old);

   if (old->index == -1)
     {
	struct ememoa_mempool_alloc_item_s	*item;

	EMEMOA_LOCK(memory);

	item = old->item;
      
	if (item->prev != NULL)
	  item->prev->next = item->next;

	if (item->next != NULL)
	  item->next->prev = item->prev;

	if (memory->start == item)
	  memory->start = item->next;

	EMEMOA_UNLOCK(memory);

	free (old);

	return ememoa_mempool_fixed_push_object (memory->allocated_list, item);
	return -1;
     }
   else
     return ememoa_mempool_fixed_push_object(memory->pools[old->index], old);
}

/**
 * Pops a new object out of the memory pool
 *
 * The following example code demonstrate how to ensure that a
 * pointer has been successfully retrived from the memory pool.
 *
 * @code
 *   object_s *new_object = ememoa_mempool_unknown_size_pop_object (&mempool_of_object, 100);
 *
 *   if (new_object == NULL)
 *   {
 *	fprintf (stderr, "ERROR: %s", ememoa_mempool_error2string (mempool_of_object.last_error_code));
 *	exit (-1);
 *   }
 * @endcode
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c pointer is not a valid memory pool,
 *					bad things will happens to your program.
 * @return	Will return @c NULL if it was impossible to allocate any data. Check
 *		memory->last_error_code and ememoa_mempool_error2string to know why.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
void*
ememoa_mempool_unknown_size_pop_object (unsigned int	mempool,
					unsigned int	size)
{
   struct ememoa_mempool_unknown_size_item_s	*new = NULL;
   unsigned int					i;
   struct ememoa_mempool_unknown_size_s		*memory = all_unknown_size_pool + mempool;

   EMEMOA_CHECK_MAGIC(memory);

   for (i = 0; i < memory->pools_count; ++i)
     if (memory->pools_match[i] > size)
       {
	  new = ememoa_mempool_fixed_pop_object (memory->pools[i]);
	  if (new == NULL)
	    {
	       memory->last_error_code = ememoa_mempool_fixed_get_last_error (memory->pools[i]);
	       return NULL;
	    }

	  new->index = i;
	  new->item = NULL;
	  break ;
       }

   if (!new)
     {
	struct ememoa_mempool_alloc_item_s	*item;

	if ((item = ememoa_mempool_fixed_pop_object (memory->allocated_list)) == NULL)
	  {
	     memory->last_error_code = ememoa_mempool_fixed_get_last_error (memory->pools[i]);
	     return NULL;
	  }

	EMEMOA_LOCK(memory);

	item->prev = NULL;
	item->next = memory->start;

	new = malloc (size);
	new->index = -1;
      
	item->data = new;
	new->item = item;

	if (memory->start)
	  memory->start->prev = item;

	memory->start = item;

	EMEMOA_UNLOCK(memory);
     }

#ifdef DEBUG
   new->magic = EMEMOA_MAGIC;
#endif
   new->data = new + 1;

   return new->data;
}

/**
 * Collects all the empty pool and resize the Mempool accordingly.
 *
 * The following example code demonstrate how to ensure that a
 * given Mempool give some memory back or die.
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c memory is not a valid memory pool, bad things
 *					will happen.
 * @return	Will return @c 0 if some pools were freed.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
int
ememoa_mempool_unknown_size_garbage_collect (unsigned int mempool)
{
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;
   unsigned int				i;
   int					count = 0;

   EMEMOA_CHECK_MAGIC(memory);

   for (i = 0; i < memory->pools_count; ++i)
     count += ememoa_mempool_fixed_garbage_collect (memory->pools[i]);
 
   count += ememoa_mempool_fixed_garbage_collect (memory->allocated_list);

   return count;  
}

/**
 * Execute fctl on all allocated data in the pool. If the execution of fctl something else
 * than 0, then the walk ends and returns the error code provided by fctl. 
 *
 * @param	memory		Pointer to a valid address of a memory pool. If
 *				@c memory is not a valid memory pool, bad things
 *				will happen.
 * @param	fctl		Function pointer that must be run on all allocated
 *				objects.
 * @param	data		Pointer that will be passed as is to each call to fctl.	
 * @return	Will return @c 0 if the run walked over all allocated objects.
 * @ingroup	Ememoa_Mempool_Unknown_Size
 */
int
ememoa_mempool_unknown_size_walk_over (unsigned int	mempool,
				       ememoa_fctl	fctl,
				       void		*data)
{
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;
   unsigned int				i;
   int					error = 0;

   EMEMOA_CHECK_MAGIC(memory);

   for (i = 0; i < memory->pools_count; ++i)
     if ((error = ememoa_mempool_fixed_walk_over (memory->pools[i], fctl, data)) != 0)
       return error;

   if ((error = ememoa_mempool_fixed_walk_over (memory->allocated_list, fctl, data)) != 0)
     return error;
  
   return 0;
}


/**
 * Displays all the statistics currently known about a Mempool, useful to dimension it.
 *
 * @param	memory			Pointer to a valid address of a memory pool. If
 *					@c memory is not a valid memory pool, bad things
 *					will happen.
 * @ingroup	Ememoa_Display_Mempool
 */
#ifdef DEBUG
void
ememoa_mempool_unknown_size_display_statistic (unsigned int mempool)
{
   struct ememoa_mempool_unknown_size_s	*memory = all_unknown_size_pool + mempool;
   unsigned int				i;

   EMEMOA_CHECK_MAGIC(memory);

   for (i = 0; i < memory->pools_count; ++i)
     ememoa_mempool_fixed_display_statistic (memory->pools[i]);

   ememoa_mempool_fixed_display_statistic (memory->allocated_list);
}
#endif