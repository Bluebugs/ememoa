#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

#include "ememoa_mempool_unknown_size.h"

static const char*	test_str = "Stupid test string";
static const char*	print_str = "Str: %s, Int: %i\n";

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
   unsigned int	test_zone;
   char*        tst[10];
   unsigned int	i;
   unsigned int	size;
   void *mem;

   mem = get_memory(MEMSIZE);
   if (ememoa_memory_base_init_64m (mem, MEMSIZE))
     return 128;

   test_zone = ememoa_mempool_unknown_size_init (sizeof(default_map_size_count)/(sizeof(unsigned int) * 2),
                                                 default_map_size_count,
                                                 0,
                                                 NULL);

   for (i = 0; i < 10; ++i)
     {
        size = strlen (test_str) + strlen (print_str) + 3;

        tst[i] = ememoa_mempool_unknown_size_pop_object (test_zone, size);

        snprintf (tst[i], size, print_str, test_str, i);
     }


   for (i = 0; i < 10; ++i)
     if (*(tst[i]) != 'S')
       return -2;


   for (i = 0; i < 10; ++i)
     ememoa_mempool_unknown_size_push_object (test_zone, tst[i]);

   ememoa_mempool_unknown_size_clean (test_zone);

   return 0;
}
