#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
void XBat_GenerateRandomString(TCHAR* pszBuffer, DWORD dwSize);
#ifdef __cplusplus
}
#endif

#ifdef MODE_CONVERTER
#include <wincrypt.h>
BOOL XBat_GenerateRandomBytes(BYTE* lpBuffer, DWORD dwSize);

#endif // MODE_CONVERTER


#endif // !UTILS_H

