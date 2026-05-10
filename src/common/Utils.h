#ifndef UTILS_H
#define UTILS_H

#include <Windows.h>

void XBat_GenerateRandomString(TCHAR* pszBuffer, DWORD dwSize);

#ifdef MODE_CONVERTER
#include <wincrypt.h>
BOOL XBat_GenerateRandomBytes(BYTE* lpBuffer, DWORD dwSize);

#endif // MODE_CONVERTER




#endif // !UTILS_H

