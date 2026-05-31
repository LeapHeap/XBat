#include "Utils.h"
#include <Windows.h>

#ifdef MODE_CONVERTER
#include <tchar.h>
#else
#include "nocrt_patch.h"
#endif //MODE_CONVERTER

void XBat_GenerateRandomString(TCHAR* pszBuffer, DWORD dwSize)
{
	if (pszBuffer == NULL || dwSize < 5) return;
	
	const TCHAR szChars[] = _T("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	
	const int nCharCount = 36; 
	
	DWORD dwSeed = GetTickCount();
	
	for (int i = 0; i < 4; i++)
	{
		dwSeed = dwSeed * 1103515245 + 12345;
		int idx = (dwSeed / 65536) % nCharCount;
		
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

