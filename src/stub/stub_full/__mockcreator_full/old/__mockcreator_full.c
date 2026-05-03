#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <stdio.h>
#include <windows.h>
#include <tchar.h>



#include "../../common/shared_defs.h"
#include "../../common/crypto.h"
#include "../../common/crypto.c"

BYTE* LoadFileToBuffer(LPCTSTR szFilePath, DWORD* pdwSize) {
	HANDLE hFile = CreateFile(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return NULL;
	
	*pdwSize = GetFileSize(hFile, NULL);
	BYTE* pBuffer = (BYTE*)malloc(*pdwSize);
	if (pBuffer) {
		DWORD dwRead;
		ReadFile(hFile, pBuffer, *pdwSize, &dwRead, NULL);
	}
	CloseHandle(hFile);
	return pBuffer;
}

BOOL PackAndInjectResource(
						   HANDLE hUpdate,
						   int ResourceID,
						   const BYTE* pRawData,
						   DWORD dwDataLen,
						   const BYTE* pKey,
						   LPCTSTR szTargetFileName,
						   DWORD dwAttrib
						   ) {
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)malloc(sizeof(XBAT_RES_HEADER) + dwDataLen);
	if (!pHeader) return FALSE;
	
	pHeader->Magic = XBAT_MAGIC_INT;
	pHeader->SavedCrc = CalculateCRC32(pRawData, dwDataLen);
	pHeader->dwOriginalSize = dwDataLen;
	pHeader->dwAttributes = dwAttrib;
	
	memset(pHeader->szFileName, 0, sizeof(pHeader->szFileName));
	if (szTargetFileName) {
		_tcscpy_s(pHeader->szFileName, MAX_PATH, szTargetFileName);
	}
	
	memcpy(pHeader->Data, pRawData, dwDataLen);
	RC4_CTX ctx;
	RC4_Init(&ctx, pKey, 16);
	RC4_Process(&ctx, pHeader->Data, dwDataLen);
	
	BOOL bRet = UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(ResourceID), 
							   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), 
							   pHeader, sizeof(XBAT_RES_HEADER) + dwDataLen - 1);
	
	free(pHeader);
	return bRet;
}

void CreateTestStub(LPCTSTR szStubPath, LPCTSTR szOutputPath) {
	XBAT_CONFIG cfg = {0};
	cfg.Magic = XBAT_MAGIC_INT;
	cfg.GlobalFlags = XBAT_FLAG_RUN_BAT_AS_FILE | XBAT_FLAG_SHOW_CONSOLE | XBAT_FLAG_HAS_USER_RESOURCES;
	cfg.DropDirType = XBAT_DROP_DIR_TEMP;
	
	const char* rawKeyString = "TEST_KEY_123456"; 
	BYTE rawKey[16] = {0}; 
	memcpy(rawKey, rawKeyString, 15); 
	BYTE obfuscatedKey[16];
	for (int i = 0; i < 16; i++) {
		obfuscatedKey[i] = rawKey[i] ^ (XBAT_KEY_OBFUSCATOR + i);
	}
	
	DWORD dwScriptLen = 0;
	BYTE* pScriptBuffer = LoadFileToBuffer(_T("test.bat"), &dwScriptLen);
	if (!pScriptBuffer) return;
	
	DWORD dwRes1Len = 0, dwRes2Len = 0;
	BYTE* pRes1 = LoadFileToBuffer(_T("test1.jpg"), &dwRes1Len);
	BYTE* pRes2 = LoadFileToBuffer(_T("test2.exe"), &dwRes2Len);
	
	if (!CopyFile(szStubPath, szOutputPath, FALSE)) {
		if (pScriptBuffer) free(pScriptBuffer);
		if (pRes1) free(pRes1);
		if (pRes2) free(pRes2);
		return;
	}
	
	HANDLE hUpdate = BeginUpdateResource(szOutputPath, FALSE);
	if (hUpdate) {
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_CONFIG), 
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &cfg, sizeof(cfg));
		
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_KEY), 
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), obfuscatedKey, 16);
		
		PackAndInjectResource(hUpdate, IDR_XBAT_BAT, pScriptBuffer, dwScriptLen, rawKey, NULL, 0);
		
		int nCurrentID = 501;
		
		if (pRes1 && nCurrentID <= 900) {
			PackAndInjectResource(hUpdate, nCurrentID++, pRes1, dwRes1Len, rawKey, _T("test1.jpg"), FILE_ATTRIBUTE_HIDDEN);
		}
		
		if (pRes2 && nCurrentID <= 900) {
			PackAndInjectResource(hUpdate, nCurrentID++, pRes2, dwRes2Len, rawKey, _T("test2.exe"), FILE_ATTRIBUTE_NORMAL);
		}
		
		EndUpdateResource(hUpdate, FALSE);
	}
	
	if (pScriptBuffer) free(pScriptBuffer);
	if (pRes1) free(pRes1);
	if (pRes2) free(pRes2);
	
	_tprintf(_T("Successfully created: %s\n"), szOutputPath);
}

int main() {
	CreateTestStub(_T("stub_full.exe"), _T("TestStub.exe"));
	return 0;
}
