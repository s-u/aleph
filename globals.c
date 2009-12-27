#include "aleph.h"

jmp_buf error_jmpbuf;

ASymbol symbol[MAX_SYM];
vlen_t  symbols = 0;

ThreadContext mainThreadContext;

AutoreleasePool *gc_pool;
