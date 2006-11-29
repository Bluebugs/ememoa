/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef		MEMPOOL_STRUCT_H__
# define	MEMPOOL_STRUCT_H__

#include	"config.h"

#include	<stdint.h>

#ifdef HAVE_PTHREAD
# include	<pthread.h>
#endif

struct ememoa_mempool_fixed_s
{
#ifdef DEBUG
   unsigned int                                 magic;
#endif

   struct ememoa_memory_base_resize_list_s      *base;
   ememoa_mempool_error_t                       last_error_code;

   unsigned int                                 object_size;
   unsigned int                                 options;

   unsigned int                                 max_objects_pot;
   unsigned int                                 max_objects_poi;
   unsigned int                                 max_objects;

   int                                          jump_pool;
   const struct ememoa_mempool_desc_s           *desc;

#ifdef DEBUG
   unsigned int                                 out_objects;
   unsigned int                                 max_out_objects;
#endif

#ifdef HAVE_PTHREAD
   pthread_mutex_t                              lock;
#endif
};

struct ememoa_mempool_unknown_size_s
{
#ifdef DEBUG
   unsigned int                                 magic;
#endif

   ememoa_mempool_error_t                       last_error_code;

   unsigned int                                 options;

   unsigned int                                 pools_count;
   unsigned int                                 *pools_match;
   unsigned int                                 *pools;

   unsigned int                                 allocated_list;

   struct ememoa_mempool_alloc_item_s           *start;

   const struct ememoa_mempool_desc_s           *desc;

#ifdef HAVE_PTHREAD
   pthread_mutex_t                              lock;
#endif

   unsigned char                                in_use;
};

struct ememoa_mempool_fixed_s*          ememoa_mempool_fixed_get_index (unsigned int index);
struct ememoa_mempool_unknown_size_s*   ememoa_mempool_unknown_size_get_index (unsigned int index);

#endif		/* MEMPOOL_STRUCT_H__ */
