#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>

#include "ememoa_mempool_unknown_size.h"

unsigned long int random_seed()
{
   struct timeval       tv;
   FILE                 *devrandom;
   unsigned int         seed = 0;

   devrandom = fopen ("/dev/random", "r");

   if (devrandom)
     if (fread (&seed, sizeof (seed), 1, devrandom) == 1)
       {
          fclose (devrandom);
          return seed;
       }

   gettimeofday(&tv,0);
   seed = tv.tv_sec + tv.tv_usec;

   return seed;
}

#define MEMSIZE 1024 * 1024
#define MAX_POOL 10

int main(void)
{
   void         *mem;
   int          *test;
   int           test_zone;
   unsigned int  count = 0;

   mem = malloc(MEMSIZE);
   if (!mem)
     return 128;

   if (ememoa_memory_base_init_64m(mem, MEMSIZE))
     return 128;

   test_zone = ememoa_mempool_fixed_init(sizeof (int), MAX_POOL, 0, NULL);

   if (test_zone < 0)
     return -1;

   test = ememoa_mempool_fixed_pop_object(test_zone);
   while (test != NULL)
     {
        count++;
        test = ememoa_mempool_fixed_pop_object(test_zone);
     }

   ememoa_mempool_fixed_clean (test_zone);

   return 0;
}
