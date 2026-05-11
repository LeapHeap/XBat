#ifndef STUB_FULL_H
#define STUB_FULL_H

#include <windows.h>

#ifdef MODE_FULL

#ifdef __cplusplus
extern "C" {
#endif

BYTE* XBat_DecompressBuffer(const BYTE* pCompressedData, DWORD dwCompressedSize, DWORD dwExpectedSize);
BOOL RunBatPipe(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR pBatPath);
void SetupConsole(LPCSTR lpszConsoleTitle);
	
#ifdef __cplusplus
}
#endif

#endif


#endif
