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
	if ((Memory.options & EMEMOA_THREAD_PROTECTION) == EMEMOA_THREAD_PROTECTION) \
		pthread_mutex_lock(&(Memory.lock));

#define	EMEMOA_UNLOCK(Memory) \
	if ((Memory.options & EMEMOA_THREAD_PROTECTION) == EMEMOA_THREAD_PROTECTION) \
		pthread_mutex_unlock(&(Memory.lock));

#else

#define EMEMOA_LOCK(Memory)	;
#define EMEMOA_UNLOCK(Memory)	;

#endif

#include "ememoa_mempool_fixed.h"
#include "mempool_struct.h"

#define	EMEMOA_MAGIC	0x4224007

#ifdef DEBUG
#define	EMEMOA_CHECK_MAGIC(Memory) \
	assert(Memory.magic == EMEMOA_MAGIC); \
	assert(Memory.in_use == 1); \
	assert(Memory.max_objects_pot >= BITMASK_POWER);
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

struct ememoa_mempool_fixed_s	*all_fixed_pool = NULL;
static int			all_fixed_pool_count = 0;
static int			all_fixed_pool_jump = 0;

/**
 * @defgroup Ememoa_Mempool_Fixed Memory pool manipulation functions for fixed size object
 *
 */

/**
 * Search or allocate a new mempool structur.
 *
 * @return	Will return the index of the mempool in all_fixed_pool or -1 if it failed.
 * @ingroup	Ememoa_Search_Mempool
 */
static int			new_ememoa_fixed_pool ()
{
   struct ememoa_mempool_fixed_s	*temp;
   int					i;

   for (i = all_fixed_pool_jump; i < all_fixed_pool_count; ++i)
     if (all_fixed_pool[i].in_use == 0)
       return i;

   ++all_fixed_pool_count;
   temp = realloc (all_fixed_pool, sizeof (struct ememoa_mempool_fixed_s) * all_fixed_pool_count);

   if (temp == NULL)
     return -1;

   all_fixed_pool = temp;

   return all_fixed_pool_count - 1;
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
int	ememoa_mempool_fixed_init (unsigned int				object_size,
				   unsigned int				preallocated_item_pot,
				   unsigned int				options,
				   const struct ememoa_mempool_desc_s	*desc)
{
   int					index = new_ememoa_fixed_pool ();
   struct ememoa_mempool_fixed_s	*memory = all_fixed_pool + index;

   if (index == -1)
     return index;

   assert(memory != NULL);
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
   memory->total_objects = 0;
   memory->magic = EMEMOA_MAGIC;
#endif

   memory->desc = desc;
   memory->last_error_code = EMEMOA_NO_ERROR;

   memory->allocated_pool = 0;
   memory->jump_pool = 0;
   memory->jump_object = NULL;
   memory->objects_pool = NULL;
   memory->available_objects = NULL;
   memory->objects_use = NULL;

   memory->options = options;
   memory->in_use = 1;

#ifdef HAVE_PTHREAD
   pthread_mutex_init (&(memory->lock), NULL);
#endif

   all_fixed_pool_jump = index + 1;

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
int	ememoa_mempool_fixed_clean (int	mempool)
{
   int	error_code = 0;

   error_code = ememoa_mempool_fixed_free_all_objects (mempool);
   if (error_code)
     return error_code;

#ifdef HAVE_PTHREAD
   pthread_mutex_destroy (&(all_fixed_pool[mempool].lock));
#endif

   bzero (all_fixed_pool + mempool, sizeof (struct ememoa_mempool_fixed_s));

   if (all_fixed_pool_jump > mempool)
     all_fixed_pool_jump = mempool;

   return 0;
}

/**
 * @defgroup Ememoa_Alloc_Mempool Helper function for object allocation
 *
 */

/**
 * Set the bit corresponding to the allocated adress as used.
 *
 * @param	index_l			Lower part of the index (inside one bitmask_t)
 * @param	index_h			Higher part of the index (index in the bitmask_t[])
 * @param	objects_use_slot	Pointer to the bitmask_t lookup table.
 * @param	jump_object_slot	Fast jump to the first available entry in the table.
 * @ingroup	Ememoa_Alloc_Mempool
 */
static void
set_adress (unsigned int	index_l,
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

#define	EMEMOA_CHECK_UPDATE_POINTER(Pointer, Value) \
	if (Pointer != NULL) \
		memory->Pointer = Pointer; \
	else \
	{ \
		memory->last_error_code = Value; \
		failed = -1; \
	}

/**
 * Allocate a new empty pool inside a Mempool
 *
 * @param	memory		Pointer to a valid address of a memory pool. If
 *				an invalid pool is passed, bad things will happen.
 * @return	Will return @c 0 if successfully allocated.
 * @ingroup	Ememoa_Alloc_Mempool
 */
static int
add_pool (struct ememoa_mempool_fixed_s *memory)
{
   unsigned int	new_pool_index, allocated_pool;
   unsigned int	*available_objects;
   unsigned int	*jump_object;
   void*	*objects_use;
   void*	*objects_pool;
   int		failed = 0;

   allocated_pool = memory->allocated_pool;
   new_pool_index = allocated_pool++;

   jump_object = realloc (memory->jump_object, allocated_pool * sizeof (unsigned int));
   available_objects = realloc (memory->available_objects, allocated_pool * sizeof (unsigned int));
   objects_use = realloc (memory->objects_use, allocated_pool * sizeof (bitmask_t*));
   objects_pool = realloc (memory->objects_pool, allocated_pool * sizeof (void*));

   EMEMOA_CHECK_UPDATE_POINTER(jump_object, EMEMOA_ERROR_REALLOC_JUMP_OBJECT_FAILED);
   EMEMOA_CHECK_UPDATE_POINTER(available_objects, EMEMOA_ERROR_REALLOC_AVAILABLE_OBJECTS_FAILED);
   EMEMOA_CHECK_UPDATE_POINTER(objects_use, EMEMOA_ERROR_REALLOC_OBJECTS_USE_FAILED);
   EMEMOA_CHECK_UPDATE_POINTER(objects_pool, EMEMOA_ERROR_REALLOC_OBJECTS_POOL_FAILED);

   if (failed)
     return -1;

   objects_use[new_pool_index] = malloc (sizeof (bitmask_t) * memory->max_objects_poi);
   objects_pool[new_pool_index] = malloc (EMEMOA_SIZEOF_POOL(memory));

   if (objects_use[new_pool_index] == NULL 
       || objects_pool[new_pool_index] == NULL)
     {
	free (objects_use[new_pool_index]);
	free (objects_pool[new_pool_index]);
	memory->last_error_code = EMEMOA_ERROR_MALLOC_NEW_POOL;

	return -1;
     }

#ifdef DEBUG
   memset (objects_pool[new_pool_index], 42, EMEMOA_SIZEOF_POOL(memory));

   memory->total_objects += memory->max_objects;
#endif

   /* Set all objects as available */
   memset (objects_use[new_pool_index], 0xFF, sizeof (bitmask_t) * memory->max_objects_poi);
   available_objects[new_pool_index] = memory->max_objects - 1;
   memory->jump_object[new_pool_index] = 0;
   memory->allocated_pool = allocated_pool;

   return 0;  
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
void*	ememoa_mempool_fixed_pop_object (int	mempool)
{
   uint8_t				*start_adress = NULL;
   unsigned int				allocated_pool;
   unsigned int				*available_objects;
   
   EMEMOA_CHECK_MAGIC(all_fixed_pool[mempool]);
   
   EMEMOA_LOCK(all_fixed_pool[mempool]);
   
   allocated_pool = all_fixed_pool[mempool].allocated_pool;
   available_objects = all_fixed_pool[mempool].available_objects;

   for (available_objects += all_fixed_pool[mempool].jump_pool;
	all_fixed_pool[mempool].jump_pool < allocated_pool && *available_objects == 0;
	++(all_fixed_pool[mempool].jump_pool), ++available_objects)
     ;

   if (all_fixed_pool[mempool].jump_pool < allocated_pool)
     {
	unsigned int	slot = all_fixed_pool[mempool].jump_pool;
	bitmask_t	*itr = all_fixed_pool[mempool].objects_use[slot];
	bitmask_t	reg;
	int		index = 0;

	/*
	  If bit is set to 0, then the corresponding memory object is in use
	  if bit is set to 1, then the corresponding memory object is available
	*/
	for (itr += all_fixed_pool[mempool].jump_object[slot];
	     *itr == 0;
	     ++(all_fixed_pool[mempool].jump_object[slot]), ++itr)
	  ;
	
	index = all_fixed_pool[mempool].jump_object[slot] << BITMASK_POWER;
	reg = *itr;

#ifdef USE64
	index += ffsll(reg) - 1;
#else
	index += ffs(reg) - 1;
#endif

	/* Remove available objects from the slot and update jump_pool if necessary */
	if (--(all_fixed_pool[mempool].available_objects[slot]) == 0 && all_fixed_pool[mempool].jump_pool == slot)
	  ++(all_fixed_pool[mempool].jump_pool);
	
	start_adress = all_fixed_pool[mempool].objects_pool[slot] + index * all_fixed_pool[mempool].object_size;

	set_adress(EMEMOA_INDEX_LOW(index),
		   EMEMOA_INDEX_HIGH(index),
		   all_fixed_pool[mempool].objects_use[slot],
		   all_fixed_pool[mempool].jump_object + slot);
     }
   else
     {
	if (add_pool(all_fixed_pool + mempool) == 0)
	  {
	     /*
	       memory->allocated_pool has been increased by one, but not allocated_pool, so
	       we use the last one as the valid index.
	      */
	     start_adress = all_fixed_pool[mempool].objects_pool[allocated_pool];
	     set_adress(0, 0,
			all_fixed_pool[mempool].objects_use[allocated_pool],
			all_fixed_pool[mempool].jump_object + allocated_pool);
	  }
	else
	  all_fixed_pool[mempool].last_error_code = EMEMOA_NO_MORE_MEMORY;
     }
   
#ifdef DEBUG
   all_fixed_pool[mempool].max_out_objects += (all_fixed_pool[mempool].max_out_objects < ++(all_fixed_pool[mempool].out_objects)) ? 1 : 0;
#endif

   EMEMOA_UNLOCK(all_fixed_pool[mempool]);
   return start_adress;
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
int	ememoa_mempool_fixed_free_all_objects (int mempool)
{
   unsigned int				i;
   void*				*objects_use;
   void*				*objects_pool;

   EMEMOA_CHECK_MAGIC(all_fixed_pool[mempool]);

   EMEMOA_LOCK(all_fixed_pool[mempool]);
  
   free (all_fixed_pool[mempool].available_objects);
   all_fixed_pool[mempool].available_objects = NULL;

   objects_use = all_fixed_pool[mempool].objects_use;
   objects_pool = all_fixed_pool[mempool].objects_pool;
   for (i = 0; i < all_fixed_pool[mempool].allocated_pool && *objects_use && *objects_pool; ++i, ++objects_use, ++objects_pool)
     {
	free (*objects_use);
	free (*objects_pool);
     }
   free (all_fixed_pool[mempool].objects_use);
   free (all_fixed_pool[mempool].objects_pool);

   all_fixed_pool[mempool].objects_use = NULL;
   all_fixed_pool[mempool].objects_pool = NULL;

#ifdef DEBUG
   all_fixed_pool[mempool].out_objects = 0;
   all_fixed_pool[mempool].total_objects = 0;
#endif

   all_fixed_pool[mempool].allocated_pool = 0;

   EMEMOA_UNLOCK(all_fixed_pool[mempool]);

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
int	ememoa_mempool_fixed_push_object (int	mempool,
					  void	*ptr)
{
   unsigned int				i;
   unsigned int				size;
   void*				*objects_pool;
   void*				ptr_inf;

   EMEMOA_CHECK_MAGIC(all_fixed_pool[mempool]);

   size = all_fixed_pool[mempool].object_size * all_fixed_pool[mempool].max_objects;
   ptr_inf = ptr - size;

   objects_pool = all_fixed_pool[mempool].objects_pool;
   for (i = 0; i < all_fixed_pool[mempool].allocated_pool;
	++i, ++objects_pool)
     {
	if (*objects_pool <= ptr && ptr_inf <= (void*)((uint8_t*) *objects_pool))
	  {
	     bitmask_t		*objects_use = all_fixed_pool[mempool].objects_use[i];
	     bitmask_t		mask = 1;
	     /*
	       High risk, if one day sizeof (unsigned long) > sizeof (void*)
	       something will go wrong...
	     */
	     unsigned long	position = ((uint8_t*) ptr - (uint8_t*) *objects_pool) / all_fixed_pool[mempool].object_size;
	     unsigned int	index_h = EMEMOA_INDEX_HIGH(position);
	     unsigned int	index_l = EMEMOA_INDEX_LOW(position);

	     mask <<= index_l;

	     EMEMOA_LOCK(all_fixed_pool[mempool]);

#ifdef DEBUG
	     (all_fixed_pool[mempool].out_objects)--;
	     if (objects_use[index_h] & mask)
	       {
		  all_fixed_pool[mempool].last_error_code = EMEMOA_DOUBLE_PUSH;
	      
		  EMEMOA_UNLOCK(all_fixed_pool[mempool]);
		  return -1;
	       }
#endif

	     objects_use[index_h] |= mask;
	     (all_fixed_pool[mempool].available_objects[i])++;

	     if (all_fixed_pool[mempool].jump_object[i] > index_h)
	       all_fixed_pool[mempool].jump_object[i] = index_h;

	     if (all_fixed_pool[mempool].jump_pool > i)
	       all_fixed_pool[mempool].jump_pool = i;

	     EMEMOA_UNLOCK(all_fixed_pool[mempool]);
	     return 0;
	  }
     }

   all_fixed_pool[mempool].last_error_code = EMEMOA_ERROR_PUSH_ADRESS_NOT_FOUND;
   return -1;
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
ememoa_used_pool (struct ememoa_mempool_fixed_s *memory)
{
   unsigned int	i;
   unsigned int	retour;

   for (i = 0, retour = 0;
	i < memory->allocated_pool;
	++i)
     if (memory->available_objects[i] != memory->max_objects)
       ++retour;
     else
       {
	  free (memory->objects_use[i]);
	  free (memory->objects_pool[i]);

	  memory->objects_use[i] = NULL;
	  memory->objects_pool[i] = NULL;
	  memory->available_objects[i] = 0;
       }

   return retour;
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
   unsigned int				i, j;
   unsigned int				allocated_pool = 0;
   unsigned int				*available_objects = NULL;
   bitmask_t				**objects_use = NULL;
   void					**objects_pool = NULL;

   EMEMOA_CHECK_MAGIC(all_fixed_pool[mempool]);

   if (all_fixed_pool[mempool].allocated_pool == 0)
     return 0;

   EMEMOA_LOCK(all_fixed_pool[mempool]);

   allocated_pool = ememoa_used_pool (all_fixed_pool + mempool);

   if (allocated_pool == all_fixed_pool[mempool].allocated_pool)
     {
	EMEMOA_UNLOCK(all_fixed_pool[mempool]);

	all_fixed_pool[mempool].last_error_code = EMEMOA_NO_EMPTY_POOL;
	return -1;
     }

   if (allocated_pool != 0)
     {
	available_objects = malloc (sizeof (unsigned int) * allocated_pool);
	objects_use = malloc (sizeof (bitmask_t*) * allocated_pool);
	objects_pool = malloc (sizeof (void*) * allocated_pool);
      
	for (i = 0, j = 0;
	     i < all_fixed_pool[mempool].allocated_pool;
	     ++i)
	  if (all_fixed_pool[mempool].objects_use[i] != NULL)
	    {
	       available_objects[j] = all_fixed_pool[mempool].available_objects[i];
	       objects_use[j] = all_fixed_pool[mempool].objects_use[i];
	       objects_pool[j] = all_fixed_pool[mempool].objects_pool[i];
	       ++j;
	    }
     }

   free (all_fixed_pool[mempool].available_objects);
   free (all_fixed_pool[mempool].objects_use);
   free (all_fixed_pool[mempool].objects_pool);

   all_fixed_pool[mempool].allocated_pool = allocated_pool;
   all_fixed_pool[mempool].available_objects =  available_objects;
   all_fixed_pool[mempool].objects_use = (void*) objects_use;
   all_fixed_pool[mempool].objects_pool = objects_pool;

#ifdef DEBUG
   all_fixed_pool[mempool].total_objects = all_fixed_pool[mempool].object_size * all_fixed_pool[mempool].allocated_pool;
#endif

   EMEMOA_UNLOCK(all_fixed_pool[mempool]);

   return 0;
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
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @return	Will return @c 0 if some pools were freed.
 * @ingroup	Ememoa_Mempool_Fixed
 */
int
ememoa_mempool_fixed_garbage_collect_all(void)
{
   int	i;
   int	result = -1;

   for (i = 0; i < all_fixed_pool_count; ++i)
     if (all_fixed_pool[i].in_use == 1)
       result &= ememoa_mempool_fixed_garbage_collect (i);

   return result;
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
   unsigned int				i, j, k;

   EMEMOA_CHECK_MAGIC(all_fixed_pool[mempool]);

   EMEMOA_LOCK(all_fixed_pool[mempool]);

   for (i = 0; i < all_fixed_pool[mempool].allocated_pool; ++i)
     {
	uint8_t		*start_adress = all_fixed_pool[mempool].objects_pool[i];
	bitmask_t	*objects_use = all_fixed_pool[mempool].objects_use[i];

	for (j = 0; j < all_fixed_pool[mempool].max_objects_poi; ++j)
	  {
	     bitmask_t	value = *objects_use++;
	     
	     for (k = 0; k < (1 << BITMASK_POWER);
		  ++k,
		    value >>= 1,
		    start_adress += all_fixed_pool[mempool].object_size)
	       if ((value & 1) == 0)
		 {
		    int	error = fctl (start_adress, data);
		    if (error)
		      {
			 EMEMOA_UNLOCK(all_fixed_pool[mempool]);
			 return error;
		      }
		 }
	  }
     }

   EMEMOA_UNLOCK(all_fixed_pool[mempool]);
   return 0;
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
static void
ememoa_mempool_fixed_display_pool (struct ememoa_mempool_fixed_s *memory, unsigned int index)
{
   unsigned int				i, j;
   char					display[64] = "";
   bitmask_t				value;
   unsigned long			bound_l, bound_h;
   
   bound_l = EMEMOA_EVAL_BOUND_LOW(memory->objects_pool[index]);
   bound_h = EMEMOA_EVAL_BOUND_HIGH(memory->objects_pool[index], (*memory));
  
   printf ("Available objects: %i\n", memory->available_objects[index]);
   printf ("Start at: %p, end at: %p\n", (void*) bound_l, (void*) bound_h);

   if (bound_l != 0)
     {
	display[(1 << BITMASK_POWER)] = '\0';
	for (i = 0; i < memory->max_objects_poi; ++i)
	  {
	     bitmask_t	*objects_use = memory->objects_use[index];
	     
	     value = objects_use[i];
	     for (j = 0; j < (1 << BITMASK_POWER); ++j, value >>= 1)
	       /*
		 if bit is set to 0, then the corresponding memory object is in use
		 if bit is set to 1, then the corresponding memory object is available
	       */
	       display[j] = (value & 1) ? '.' : '|';

	     if (i & 1)
	       printf("%s\n", display);
	     else
	       printf("%s", display);
	  }
	if (i & 1)
	  printf("\n");
     }
}

/**
 * Displays all the statistic currently known about a Mempool, usefull to dimension it.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @ingroup	Ememoa_Display_Mempool
 */
void
ememoa_mempool_fixed_display_statistic (int	mempool)
{
   unsigned int				i;
   
   printf ("Memory information for pool located at : %p\n", (void*) all_fixed_pool + mempool);
   
   EMEMOA_LOCK(all_fixed_pool[mempool]);

   if (all_fixed_pool[mempool].desc && all_fixed_pool[mempool].desc->name)
     printf ("This pool contains : %s.\n", all_fixed_pool[mempool].desc->name);

#ifdef	DEBUG
   printf ("Memory magic is : %x\n", all_fixed_pool[mempool].magic);
   if (all_fixed_pool[mempool].magic != EMEMOA_MAGIC)
     return ;
#endif

   if (all_fixed_pool[mempool].in_use != 1)
     return ;

   printf ("Objects per pool: %i[%x] (for poi: %i, pot: %i)\n", all_fixed_pool[mempool].max_objects, all_fixed_pool[mempool].max_objects, all_fixed_pool[mempool].max_objects_poi, all_fixed_pool[mempool].max_objects_pot);
   printf ("Object size: %i\n", all_fixed_pool[mempool].object_size);
   printf ("Allocated pool: %i\n", all_fixed_pool[mempool].allocated_pool);

#ifdef DEBUG
   printf ("Objects currently delivered: %i.\n", all_fixed_pool[mempool].out_objects);
   printf ("Total objects currently in pool: %i.\n", all_fixed_pool[mempool].total_objects);
   printf ("Maximum delivered objects since the birth of the memory pool: %i.\n", all_fixed_pool[mempool].max_out_objects);
#endif

   for (i = 0; i < all_fixed_pool[mempool].allocated_pool; ++i)
     {
	printf ("== %i ==\n", i);
	ememoa_mempool_fixed_display_pool(all_fixed_pool + mempool, i);
     }

   if (all_fixed_pool[mempool].desc)
     {
	char*	name = alloca (strlen(all_fixed_pool[mempool].desc->name ? all_fixed_pool[mempool].desc->name : ""));
	strcpy (name, all_fixed_pool[mempool].desc->name);

	if (all_fixed_pool[mempool].desc->data_display)
	  {
	     printf ("=== Content ===\n");
	     ememoa_mempool_fixed_walk_over (mempool, all_fixed_pool[mempool].desc->data_display, name);
	     printf ("=== ===\n");
	  }
     }

   EMEMOA_UNLOCK(all_fixed_pool[mempool]);
}

/**
 * Displays all the statistic currently known about all active Mempool.
 *
 * @param	mempool		Index of a valid memory pool. If the pool was already clean
 *				bad things will happen to your program.
 * @ingroup	Ememoa_Display_Mempool
 */
void
ememoa_mempool_fixed_display_statistic_all(void)
{
   int	i = 0;
   int	result = 0;

   for (i = 0; i < all_fixed_pool_count; ++i)
     if (all_fixed_pool[i].in_use == 1)
       ++result;

   printf ("%i mempool are used in %i currently allocated.\n", result, all_fixed_pool_count);

   for (i = 0; i < all_fixed_pool_count; ++i)
     if (all_fixed_pool[i].in_use == 1)
       ememoa_mempool_fixed_display_statistic (i);
}

