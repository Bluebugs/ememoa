#include <stdlib.h>
#include <stdio.h>

#include "ememoa_mempool_fixed.h"

unsigned int	test_zone;

int test2_run (unsigned int limit)
{
  unsigned int		i;
  unsigned int**	tbl;

  tbl = (unsigned int**) malloc (sizeof (unsigned int*) * limit);

  for (i = 0; i < limit; ++i)
    {
      tbl[i] = ememoa_mempool_fixed_pop_object (test_zone);
      if (tbl[i] == NULL)
	{
	  fprintf (stderr, "ERROR: %s\n", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (test_zone)));
	  return 1;
	}
    }

  for (i = 0; i < limit; ++i)
    *(tbl[i]) = i;

  for (i = 0; i < limit; ++i)
    if (*(tbl[i]) != i)
      {
	fprintf (stderr, "ERROR: %i != %i.\n", *(tbl[i]), i);
	return 1;
      }

  for (i = 0; i < limit / 2; ++i)
    if (ememoa_mempool_fixed_push_object (test_zone, tbl[i]) != 0)
      {
	fprintf (stderr, "ERROR: %s [%i]\n", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (test_zone)), i);
	return 1;
      }

  ememoa_mempool_fixed_garbage_collect (test_zone);

  for (i = limit / 2; i < limit; ++i)
    if (*(tbl[i]) != i)
      {
	fprintf (stderr, "ERROR: %i != %i.\n", *(tbl[i]), i);
	return 1;
      }

  ememoa_mempool_fixed_display_statistic (test_zone);

  ememoa_mempool_fixed_free_all_objects (test_zone);

  free (tbl);
  return 0;
}

/* Pool size: 2^10 objects */
#define MAX_POOL 10

int main (void)
{
   test_zone = ememoa_mempool_fixed_init (sizeof (int), MAX_POOL, 0, NULL);

   if (test2_run (100))
     return 1;
   if (test2_run (2000))
     return 1;
   if (test2_run (1024))
     return 1;
   if (test2_run (4096))
     return 1;

   if (ememoa_mempool_fixed_clean (test_zone))
     return 1;

   return 0;
}
