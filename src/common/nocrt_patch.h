#ifndef NOCRT_PATCH_H
#define NOCRT_PATCH_H

#include <windows.h>

// ======================================================================
// 1. Memory management
// ======================================================================
#undef ZeroMemory
#undef RtlZeroMemory
#undef memset
#undef memcpy
#undef memmove

#define RtlZeroMemory(dest, size)    memset((dest), 0, (size))
#define ZeroMemory(dest, size)       memset((dest), 0, (size))
#define zero_memory(dest, size)      memset((dest), 0, (size))

#define memcpy(dest, src, size)      RtlCopyMemory((dest), (src), (size))
#define memmove(dest, src, size)     RtlMoveMemory((dest), (src), (size))

// ======================================================================
// 2. Mapping for code
// ======================================================================
#undef malloc
#undef free
#undef realloc
#define malloc(size)       HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size))
#define free(ptr)          do { if(ptr) HeapFree(GetProcessHeap(), 0, (ptr)); } while(0)
#define realloc(ptr, size) (ptr ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (ptr), (size)) : HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size)))


// ======================================================================
// 4. String searcher
// ======================================================================
#undef _tcsrchr

#include <shlwapi.h>
#ifdef UNICODE
#define _tcsrchr(str, ch)    StrRChrW((str), NULL, (ch))
#else
#define _tcsrchr(str, ch)    StrRChrA((str), NULL, (ch))
#endif

// ======================================================================
// 5. _T Macro
// ======================================================================
#ifndef _TEXT
#ifdef UNICODE
#define _TEXT(x) L##x
#else
#define _TEXT(x) x
#endif
#endif

#ifndef _T
#define _T(x) _TEXT(x)
#endif

#endif
