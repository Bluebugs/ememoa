#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

#include "ememoa_mempool_unknown_size.h"

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

#define MEMSIZE 8 * 1024 * 1024

int main(void)
{
   unsigned int	test_zone;
   uint8_t      *tst[2048];
   unsigned int	i;
   unsigned int	j;
   void         *mem;

   mem = get_memory(MEMSIZE);
   if (ememoa_memory_base_init_64m (mem, MEMSIZE))
     return 128;

   test_zone = ememoa_mempool_unknown_size_init (sizeof(default_map_size_count)/(sizeof(unsigned int) * 2),
                                                 default_map_size_count,
                                                 0,
                                                 NULL);

   for (i = 0; i < 2048; ++i)
     {
        tst[i] = ememoa_mempool_unknown_size_pop_object (test_zone, i);
        memset (tst[i], i & 0xFF, i);
     }

   for (i = 0; i < 2048; ++i)
     {
        tst[i] = ememoa_mempool_unknown_size_resize_object (test_zone, tst[i], i + 2);
        tst[i][i] = (i + 2) & 0xFF;
        tst[i][i + 1] = (i + 2) & 0xFF;
     }

   for (i = 0; i < 2048; ++i)
     {
        for (j = 0; j < i; ++j)
          if (tst[i][j] != (i & 0xFF))
            return -2;
        if (tst[i][j] != ((i + 2) & 0xFF))
          return -3;
        if (tst[i][j + 1] != ((i + 2) & 0xFF))
          return -4;
     }

   for (i = 0; i < 2048; ++i)
     ememoa_mempool_unknown_size_push_object (test_zone, tst[i]);

   ememoa_mempool_unknown_size_clean (test_zone);

   return 0;
}
