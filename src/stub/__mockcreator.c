#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "../common/shared_defs.h"
#include "../common/crypto.h" 
#include "../common/crypto.c"

void CreateTestStub(LPCTSTR szStubPath, LPCTSTR szOutputPath) {
	XBAT_CONFIG cfg = {0};
	cfg.Magic = XBAT_MAGIC_INT;
	cfg.GlobalFlags = XBAT_FLAG_RUN_BAT_AS_FILE | XBAT_FLAG_SHOW_CONSOLE | XBAT_FLAG_USE_TEMP_DROP_PATH;
	
	const char* rawKeyString = "TEST_KEY_123456"; 
	BYTE rawKey[16] = {0}; 
	memcpy(rawKey, rawKeyString, 15); 
	
	BYTE obfuscatedKey[16];
	for (int i = 0; i < 16; i++) {
		obfuscatedKey[i] = rawKey[i] ^ (XBAT_KEY_OBFUSCATOR + i);
	}
	
	const char* szScript = "@echo off\x0d\x0a" "echo [XBAT MOCK] Success!\x0d\x0a" "pause";
	DWORD dwLen = (DWORD)strlen(szScript);
	
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)malloc(sizeof(XBAT_RES_HEADER) + dwLen);
	if (!pHeader) return;
	
	pHeader->Magic = XBAT_MAGIC_INT;
	pHeader->SavedCrc = CalculateCRC32((BYTE*)szScript, dwLen);
	pHeader->OriginalSize = dwLen;
	memcpy(pHeader->Data, szScript, dwLen);
	
	RC4_CTX ctx;
	RC4_Init(&ctx, rawKey, 16);
	RC4_Process(&ctx, pHeader->Data, dwLen);
	
	if (!CopyFile(szStubPath, szOutputPath, FALSE)) {
		_tprintf(_T("Error: CopyFile failed.\n"));
		free(pHeader);
		return;
	}
	
	HANDLE hUpdate = BeginUpdateResource(szOutputPath, FALSE);
	if (hUpdate) {
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_CONFIG), 
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &cfg, sizeof(cfg));
		
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_KEY), 
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), obfuscatedKey, 16);
		
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_BAT), 
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), pHeader, sizeof(XBAT_RES_HEADER) + dwLen - 1);
		
		EndUpdateResource(hUpdate, FALSE);
	}
	
	free(pHeader);
	_tprintf(_T("Successfully created: %s\n"), szOutputPath);
}

int main() {
	CreateTestStub(_T("stub_full.exe"), _T("TestStub.exe"));
	return 0;
}
