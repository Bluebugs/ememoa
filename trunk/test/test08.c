#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "ememoa_mempool_fixed.h"

unsigned int	test_zone;

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
   unsigned int*	tbl[10];
   void *mem;

   mem = get_memory(MEMSIZE);
   if (ememoa_memory_base_init_64m (mem, MEMSIZE))
     return 128;

   test_zone = ememoa_mempool_fixed_init (sizeof (int), 10, 0, NULL);

   tbl[0] = ememoa_mempool_fixed_pop_object (test_zone);
   *(tbl[0]) = 1;
   ememoa_mempool_fixed_push_object (test_zone, tbl[0]);

   ememoa_mempool_fixed_clean (test_zone);
   return 0;
}
