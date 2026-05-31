#ifndef NOCRT_PATCH_H
#define NOCRT_PATCH_H

#include <windows.h>



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
#undef strrchr
#define strrchr(str, ch)     StrRChrA((str), NULL, (ch))


// ======================================================================
// 5. _T and _TEXT Macros - Map to Windows TEXT macro
// ======================================================================

// Ensure TEXT macro is available (provided by windows.h)
#ifndef TEXT
#ifdef UNICODE
#define TEXT(x) L##x
#else
#define TEXT(x) x
#endif
#endif

// Map _TEXT to TEXT for C runtime compatibility
#ifndef _TEXT
#define _TEXT(x) TEXT(x)
#endif

// Map _T to TEXT for TCHAR compatibility
#ifndef _T
#define _T(x) TEXT(x)
#endif

#endif

