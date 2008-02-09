#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>

#include "ememoa_mempool_unknown_size.h"

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

   test_zone = ememoa_mempool_unknown_size_init (sizeof(default_map_size_count)/(sizeof(unsigned int) * 2),
                                                 default_map_size_count,
                                                 0,
                                                 NULL);


   if (test_zone < 0)
     return -1;

   test = ememoa_mempool_unknown_size_pop_object(test_zone, 10);
   while (test != NULL)
     {
        count++;
        test = ememoa_mempool_unknown_size_pop_object(test_zone, 10);
     }

   ememoa_mempool_unknown_size_clean(test_zone);

   return 0;
}
