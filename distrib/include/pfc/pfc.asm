%include "x86inc.asm"

; Used in profiler.cpp (profiler_local::get_timestamp)
cglobal profiler_local_get_timestamp, 0, 0
    rdtsc
    RET
