#ifndef STUB_FULL_H
#define STUB_FULL_H

#include <windows.h>

#if !defined(MODE_VC6) && !defined(MODE_FULL) && !defined(BUILDING_LITE)
// For editor preview
#define MODE_FULL
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#endif

#ifdef MODE_FULL

#ifdef __cplusplus
extern "C" {
#endif

BYTE* XBat_DecompressBuffer(const BYTE* pCompressedData, DWORD dwCompressedSize, DWORD dwExpectedSize);
BOOL RunBatPipe(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR pBatPath);
	
#ifdef __cplusplus
}
#endif

#endif


#endif
