/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef		EMEMOA_MEMPOOL_FIXED_H__
# define	EMEMOA_MEMPOOL_FIXED_H__

/*
 * @file
 * @brief This routine provide memory pool manipulation for fixed size structure
 */

#include	"ememoa_mempool.h"
#include	"ememoa_mempool_struct.h"

#define	EMEMOA_THREAD_PROTECTION	1

int	ememoa_mempool_fixed_init (unsigned int				object_size,
				   unsigned int				preallocated_item,
				   unsigned int				options,
				   const struct ememoa_mempool_desc_s	*desc);

int	ememoa_mempool_fixed_clean (int					mempool);

int	ememoa_mempool_fixed_free_all_objects (int			mempool);

int	ememoa_mempool_fixed_push_object (int				mempool,
					  void				*ptr);

void*	ememoa_mempool_fixed_pop_object (int				mempool);

#ifdef DEBUG
void	ememoa_mempool_fixed_display_statistic (int			mempool);
void	ememoa_mempool_fixed_display_statistic_all (void);
#else
#define ememoa_mempool_fixed_display_statistic(Mempool) ;
#define	ememoa_mempool_fixed_display_statistic_all()	;
#endif

int	ememoa_mempool_fixed_garbage_collect(int			mempool);

int	ememoa_mempool_fixed_walk_over(int				mempool,
				       ememoa_fctl			fctl,
				       void				*data);
ememoa_mempool_error_t	ememoa_mempool_fixed_get_last_error (int	mempool);

#endif		/* EMEMOA_MEMPOOL_FIXED_H__ */
