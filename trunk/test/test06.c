#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

#include "ememoa_mempool_fixed.h"

unsigned int	test_zone;

int test1_run(unsigned int limit)
{
   unsigned int		i;
   unsigned int**	tbl;

   tbl = (unsigned int**) malloc (sizeof (unsigned int*) * limit);

   for (i = 0; i < limit; ++i)
     {
	tbl[i] = ememoa_mempool_fixed_pop_object (test_zone);
	ememoa_mempool_fixed_display_statistic (test_zone);
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
	  return 2;
       }

   for (i = 0; i < limit; ++i)
     if (ememoa_mempool_fixed_push_object (test_zone, tbl[i]) != 0)
       {
	  fprintf (stderr, "ERROR: %s [%i for %p]\n", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (test_zone)), i, tbl[i]);
	  return 3;
       }

   ememoa_mempool_fixed_display_statistic (test_zone);

   free (tbl);
   return 0;
}

/* Pool size: 2^10 objects */
#define MAX_POOL 10

static void *get_memory(size_t size)
{
   void *p;

   p = mmap(NULL, size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

   if (p == MAP_FAILED)
     {
        perror("mmap");
        fprintf(stderr, "MMAP failed!\n");
        return NULL;
     }

   return p;
}

#define MEMSIZE 4 * 1024 * 1024

int main(void)
{
   void *mem;
   int  retour = 0;

   mem = get_memory(MEMSIZE);
   if (ememoa_memory_base_init_64m (mem, MEMSIZE))
     return 128;

   test_zone = ememoa_mempool_fixed_init (sizeof (int), MAX_POOL, 0, NULL);

   if ((retour = test1_run(2000)) != 0)
     return retour;
   if ((retour = test1_run (3000)) != 0)
     return retour;
   if (ememoa_mempool_fixed_garbage_collect (test_zone))
     {
	fprintf (stderr, "ERROR: %s.\n", ememoa_mempool_error2string (ememoa_mempool_fixed_get_last_error (test_zone)));
	return 6;
     }
   if ((retour = test1_run (100)) != 0)
     return retour;

   return 0;
}
