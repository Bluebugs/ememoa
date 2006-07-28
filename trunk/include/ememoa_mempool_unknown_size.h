/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef		EMEMOA_MEMPOOL_UNKNOWN_SIZE_H__
# define	EMEMOA_MEMPOOL_UNKNOWN_SIZE_H__

/*
 * @file
 * @brief This routine provide memory pool manipulation for variable length structure, typicaly char*
 */

#include	"ememoa_mempool.h"
#include	"ememoa_mempool_struct.h"

static const unsigned int	default_map_size_count[] =
  {
    16,		9,
    32,		8,
    64,		7,
    128,	6,
    256,	5,
    512,	5,
    1024,	5,
  };

unsigned int	ememoa_mempool_unknown_size_init (unsigned int				map_items_count,
						  const unsigned int			*map_size_count,
						  unsigned int				options,
						  const struct ememoa_mempool_desc_s	*desc);

int	ememoa_mempool_unknown_size_clean (unsigned int					mempool);

int	ememoa_mempool_unknown_size_free_all_objects (unsigned int			mempool);

int	ememoa_mempool_unknown_size_push_object (unsigned int				mempool,
						 void					*ptr);

void*	ememoa_mempool_unknown_size_pop_object (unsigned int				mempool,
						unsigned int				size);

#ifdef DEBUG
void	ememoa_mempool_unknown_size_display_statistic (unsigned int			mempool);
#else
#define ememoa_mempool_unknown_size_display_statistic(Mempool) ;
#endif

int	ememoa_mempool_unknown_size_garbage_collect (unsigned int			mempool);

int	ememoa_mempool_unknown_size_walk_over (unsigned int				mempool,
					       ememoa_fctl				fctl,
					       void					*data);

ememoa_mempool_error_t	ememoa_mempool_unknown_size_get_last_error (unsigned int	mempool);

#endif		/* EMEMOA_MEMPOOL_UNKNOWN_SIZE_H__ */

