#ifndef VC6_PATCH_H
#define VC6_PATCH_H

#ifdef MODE_VC6

#include <stdio.h>
#include <windows.h>
#include <tchar.h>


#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#ifndef ULONG_PTR
#define ULONG_PTR unsigned long
#endif

#ifndef LONG_PTR
#define LONG_PTR long
#endif

#ifndef IS_INTRESOURCE
#define IS_INTRESOURCE(_r) ((((ULONG_PTR)(_r)) >> 16) == 0)
#endif

#ifndef _countof
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

#ifndef freopen_s
#ifdef __cplusplus
extern "C" {
#endif
	static int freopen_s(FILE** pFile, const char* path, const char* mode, FILE* stream) {
		if (pFile == NULL || path == NULL || mode == NULL || stream == NULL) return 22; // EINVAL
		*pFile = freopen(path, mode, stream);
		return (*pFile != NULL) ? 0 : 1;
	}
#ifdef __cplusplus
}
#endif
#endif

#endif

// String safter macro
#define SET_STOPPER(buf, size) ((buf)[(size) - 1] = _T('\0'))
#define SAFE_LEN(x) ((x) - 1)

#endif
