/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef		EMEMOA_MEMPOOL_H__
# define	EMEMOA_MEMPOOL_H__

/*
 * @file
 * @brief This routine provide general routine for the manipulation of memory pool
 */

#include	"ememoa_mempool_error.h"

const char*	ememoa_mempool_error2string (ememoa_mempool_error_t	error_code);

typedef int		(*ememoa_fctl) (void*	ptr,
					void*	data);

#endif		/* EMEMOA_MEMPOOL_H__ */
