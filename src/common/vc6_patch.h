#ifndef VC6_PATCH_H
#define VC6_PATCH_H

#ifdef MODE_VC6
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

#define strcpy_s(a, b, c) strcpy(a, c)
#define _tcscpy_s(a, b, c) _tcscpy(a, c)
#define wcscpy_s(a, b, c) wcscpy(a, c)

#define sprintf_s sprintf
#define _stprintf_s _stprintf
#endif

#endif
