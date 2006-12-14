#include <string.h>
#include <stdio.h>

#include "ememoa_mempool_unknown_size.h"

static const char*	test_str = "Stupid test string";
static const char*	print_str = "Str: %s, Int: %i\n";


int main(void)
{
   unsigned int	test_zone[80];
   unsigned int attempt;

   for (attempt = 0; attempt < 80; ++attempt)
     {
        char*		tst[10];
        unsigned int	i;
        unsigned int	size;

        test_zone[attempt] = ememoa_mempool_unknown_size_init (sizeof(default_map_size_count)/(sizeof(unsigned int) * 2),
                                                               default_map_size_count,
                                                               0,
                                                               NULL);

        for (i = 0; i < 10; ++i)
          {
             size = strlen (test_str) + strlen (print_str) + 3;

             tst[i] = ememoa_mempool_unknown_size_pop_object (test_zone[attempt], size);

             snprintf (tst[i], size, print_str, test_str, i);
          }


        for (i = 0; i < 10; ++i)
          if (*(tst[i]) != 'S')
            return -2;

        for (i = 0; i < 10; ++i)
          ememoa_mempool_unknown_size_push_object (test_zone[attempt], tst[i]);
     }

   for (attempt = 0; attempt < 80; ++attempt)
     ememoa_mempool_unknown_size_clean(test_zone[attempt]);

   return 0;
}
