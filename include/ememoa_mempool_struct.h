/*
** Copyright Cedric BAIL, 2006
** contact: cedric.bail@free.fr
**
*/

#ifndef		EMEMOA_MEMPOOL_STRUCT_H__
# define	EMEMOA_MEMPOOL_STRUCT_H__

#include	<stdint.h>

#ifdef HAVE_PTHREAD
# include	<pthread.h>
#endif

#include	"ememoa_mempool.h"
#include	"ememoa_mempool_error.h"

/*
 * @file
 * @brief This structure are used by memory pool routine
 */

struct ememoa_mempool_desc_s
{
   const char	*name;
   ememoa_fctl	data_display;
};

struct ememoa_mempool_fixed_s;
struct ememoa_mempool_alloc_item_s;
struct ememoa_mempool_unknown_size_s;

#endif		/* EMEMOA_MEMPOOL_STRUCT_H__ */
