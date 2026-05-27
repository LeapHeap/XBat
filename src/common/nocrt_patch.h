#ifndef NOCRT_PATCH_H
#define NOCRT_PATCH_H

#include <windows.h>

// ======================================================================
// 1. 彻底拦截并消灭微软原生的 内存/结构体 清零链条
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
// 2. 堆内存白嫖映射 (无 CRT 核心安全基础)
// ======================================================================
#undef malloc
#undef free
#undef realloc
#define malloc(size)       HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size))
#define free(ptr)          do { if(ptr) HeapFree(GetProcessHeap(), 0, (ptr)); } while(0)
#define realloc(ptr, size) (ptr ? HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (ptr), (size)) : HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size)))

// ======================================================================
// 3. VC6 wsprintf
// ======================================================================
#ifdef MODE_VC6

#undef wnsprintf
#undef wnsprintfA
#undef wnsprintfW
#undef _stprintf_s

static int __cdecl vc6_wnsprintfA(LPSTR lpOut, int cchLimit, LPCSTR lpFmt, ...) {
	int ret;
	va_list args;
	va_start(args, lpFmt);
	ret = wvsprintfA(lpOut, lpFmt, args);
	va_end(args);
	return ret;
}

static int __cdecl vc6_wnsprintfW(LPWSTR lpOut, int cchLimit, LPCWSTR lpFmt, ...) {
	int ret;
	va_list args;
	va_start(args, lpFmt);
	ret = wvsprintfW(lpOut, lpFmt, args);
	va_end(args);
	return ret;
}

// 将原本的 API 直接无参替换为我们的内部伪装函数，100% 避开类型转换报错！
#define wnsprintfA    vc6_wnsprintfA
#define wnsprintfW    vc6_wnsprintfW

#ifdef UNICODE
#define wnsprintf      vc6_wnsprintfW
#define _stprintf_s    vc6_wnsprintfW
#else
#define wnsprintf      vc6_wnsprintfA
#define _stprintf_s    vc6_wnsprintfA
#endif

#endif // MODE_VC6

// ======================================================================
// 4. String searcher
// ======================================================================
#undef _tcsrchr

#ifdef MODE_VC6
#ifdef UNICODE
static wchar_t* vc6_wcsrchr(const wchar_t* str, wchar_t ch) {
	wchar_t* ret = 0; while(*str) { if(*str == ch) ret = (wchar_t*)str; str++; } return ret;
}
#define _tcsrchr       vc6_wcsrchr
#else
static char* vc6_strrchr(const char* str, char ch) {
	char* ret = 0; while(*str) { if(*str == ch) ret = (char*)str; str++; } return ret;
}
#define _tcsrchr       vc6_strrchr
#endif
#else
#include <shlwapi.h>
#ifdef UNICODE
#define _tcsrchr(str, ch)    StrRChrW((str), NULL, (ch))
#else
#define _tcsrchr(str, ch)    StrRChrA((str), NULL, (ch))
#endif
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
