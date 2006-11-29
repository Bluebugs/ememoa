#include <stdlib.h>

#include "ememoa_mempool_fixed.h"

unsigned int	test_zone;

int main(void)
{
  unsigned int*	tbl[10];

  test_zone = ememoa_mempool_fixed_init (sizeof (int), 10, 0, NULL);

  tbl[0] = ememoa_mempool_fixed_pop_object (test_zone);
  
  *(tbl[0]) = 1;
  
  ememoa_mempool_fixed_push_object (test_zone, tbl[0]);

  ememoa_mempool_fixed_clean (test_zone);
  return 0;
}
