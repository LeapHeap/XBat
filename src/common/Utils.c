#include "Utils.h"
#include <Windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "vc6_patch.h"

void XBat_GenerateRandomString(TCHAR* pszBuffer, DWORD dwSize)
{
	if (pszBuffer == NULL || dwSize < 5) return;

	const TCHAR szChars[] = _T("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	const int nCharCount = _tcslen(szChars);

	for (int i = 0; i < 4; i++)
	{
		int idx = rand() % nCharCount;
		pszBuffer[i] = szChars[idx];
	}
	pszBuffer[4] = _T('\0');
}

#ifdef MODE_CONVERTER
#include <wincrypt.h>

BOOL XBat_GenerateRandomBytes(BYTE* lpBuffer, DWORD dwSize) {
	HCRYPTPROV hProv = 0;

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
		return FALSE;
	}

	BOOL bResult = CryptGenRandom(hProv, dwSize, lpBuffer);

	CryptReleaseContext(hProv, 0);

	return bResult;
}

#endif // 

