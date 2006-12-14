#include <stdlib.h>
#include "ememoa_mempool_error.h"
#include "mempool_struct.h"

const char*
ememoa_mempool_error2string (ememoa_mempool_error_t error_code)
{
  switch (error_code)
    {
    case EMEMOA_NO_ERROR:
      return "No error.";
    case EMEMOA_ERROR_INIT_ALREADY_DONE:
      return "Memory pool has already been initialized.";
    case EMEMOA_ERROR_REALLOC_AVAILABLE_OBJECTS_FAILED:
      return "Realloc of memory->available_objects failed.";
    case EMEMOA_ERROR_REALLOC_OBJECTS_USE_FAILED:
      return "Realloc of memory->objects_use failed.";
    case EMEMOA_ERROR_REALLOC_JUMP_OBJECT_FAILED:
      return "Realloc of memory->jump_object failed.";
    case EMEMOA_ERROR_REALLOC_OBJECTS_POOL_FAILED:
      return "Realloc of memory->objects_pool failed.";
    case EMEMOA_ERROR_MALLOC_NEW_POOL:
      return "New pool allocation failed.";
    case EMEMOA_ERROR_PUSH_ADDRESS_NOT_FOUND:
      return "Address doesn't belong to the pool.";
    case EMEMOA_NO_EMPTY_POOL:
      return "All pool still have some allocated objects. Impossible to give back any pool to the system.";
    case EMEMOA_INVALID_MEMPOOL:
       return "Invalid memory pool index.";
    default:
      return "Unknown error code !!";
    }
  return NULL;
}

ememoa_mempool_error_t
ememoa_mempool_fixed_get_last_error (unsigned int	mempool)
{
   struct ememoa_mempool_fixed_s	*memory = ememoa_mempool_fixed_get_index(mempool);
   return memory->last_error_code;
}


extern struct ememoa_mempool_unknown_size_s	*all_unknown_size_pool;

ememoa_mempool_error_t
ememoa_mempool_unknown_size_get_last_error (unsigned int	mempool)
{
   struct ememoa_mempool_unknown_size_s	*memory = ememoa_mempool_unknown_size_get_index (mempool);
   return (memory) ? memory->last_error_code : EMEMOA_INVALID_MEMPOOL;
}
