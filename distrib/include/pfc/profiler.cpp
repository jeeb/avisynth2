#include "pfc.h"

extern __int64 profiler_local_get_timestamp();

__int64 profiler_local::get_timestamp()
{
    return profiler_local_get_timestamp();
}
