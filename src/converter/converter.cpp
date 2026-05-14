#include <stdio.h>
#include <Windows.h>
#include <tchar.h>
#include <time.h>
#include <vector>

// MODE_CONVERTER SHOULD BE DEFINED GLOBALLY FOR THE PROJECT IN IDE
#ifndef MODE_CONVERTER
#define MODE_CONVERTER
#endif // !MODE_CONVERTER


#define DEBUG


#ifdef __cplusplus
extern "C" {
#endif
#include "../common/shared_defs.h"
#include "../common/lzma_sdk/C/LzmaEnc.h"
#include "../common/crypto.h"
#include "../common/Utils.h"
#ifdef __cplusplus
}
#endif

typedef struct {
	TCHAR szFilePath[MAX_PATH];
	//DWORD dwResourceID;
	DWORD dwFileSize;
	DWORD dwFileAttribute;

} XBAT_RESOURCE;

std::vector<XBAT_RESOURCE> g_ResList;
TCHAR g_szScriptPath[MAX_PATH];

static void* SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void* address) { free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static BYTE* LoadFileToBuffer(LPCTSTR lpszFilePath, DWORD* pdwSize) {
	HANDLE hFile = CreateFile(lpszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return NULL;

	*pdwSize = GetFileSize(hFile, NULL);
	BYTE* pBuffer = (BYTE*)malloc(*pdwSize);
	if (pBuffer) {
		DWORD dwRead;
		(void)ReadFile(hFile, pBuffer, *pdwSize, &dwRead, NULL);
	}
	CloseHandle(hFile);
	return pBuffer;
}

static BYTE* CompressDataLZMA(const BYTE* pRawData, DWORD dwRawLen, DWORD* pOutCompressedLen) {
	SizeT destLen = (SizeT)dwRawLen + dwRawLen / 8 + 65536;
	BYTE* pDestBuf = (BYTE*)malloc(destLen);
	if (!pDestBuf) return NULL;

	CLzmaEncProps props;
	LzmaEncProps_Init(&props);
	props.level = 9;
	props.dictSize = 1 << 24;
	props.writeEndMark = 0;

	SizeT propsSize = LZMA_PROPS_SIZE;
	BYTE propsEncoded[LZMA_PROPS_SIZE];

	SizeT outSizeProcessed = destLen - LZMA_PROPS_SIZE;

	SRes res = LzmaEncode(
		pDestBuf + LZMA_PROPS_SIZE, &outSizeProcessed,
		pRawData, (SizeT)dwRawLen,
		&props,
		pDestBuf, &propsSize,
		props.writeEndMark,
		NULL,                                           // progress callback
		&g_Alloc, &g_Alloc
	);

	if (res != SZ_OK) {
		free(pDestBuf);
		return NULL;
	}

	*pOutCompressedLen = (DWORD)(outSizeProcessed + LZMA_PROPS_SIZE);
	return pDestBuf;
}

static BOOL PackAndInjectResourceEx(
	HANDLE hUpdate,
	int ResourceID,
	const BYTE* pRawData,
	DWORD dwDataLen,
	const BYTE* pKey,
	LPCTSTR szTargetFileName,
	DWORD dwAttrib,
	BOOL bCompress
) {
	BYTE* pFinalDataToEncrypt = (BYTE*)pRawData;
	DWORD dwFinalDataLen = dwDataLen;
	BYTE* pCompressedBuffer = NULL;


	if (bCompress) {
		pCompressedBuffer = CompressDataLZMA(pRawData, dwDataLen, &dwFinalDataLen);
		if (pCompressedBuffer) {
			pFinalDataToEncrypt = pCompressedBuffer;
		}
	}


	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)malloc(sizeof(XBAT_RES_HEADER) + dwFinalDataLen);
	if (!pHeader) {
		if (pCompressedBuffer) free(pCompressedBuffer);
		return FALSE;
	}

	pHeader->Magic = XBAT_MAGIC_INT;
	pHeader->SavedCrc = CalculateCRC32(pFinalDataToEncrypt, dwFinalDataLen);
	pHeader->dwOriginalSize = dwDataLen;
	pHeader->dwAttributes = dwAttrib;

	memset(pHeader->szFileName, 0, sizeof(pHeader->szFileName));
	if (szTargetFileName) {
		_tcscpy_s(pHeader->szFileName, XBAT_RES_FILE_NAME_LENGTH, szTargetFileName);
	}

	memcpy(pHeader->Data, pFinalDataToEncrypt, dwFinalDataLen);
	RC4_CTX ctx;
	RC4_Init(&ctx, pKey, 16);
	RC4_Process(&ctx, pHeader->Data, dwFinalDataLen);

	BOOL bRet = UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(ResourceID),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		pHeader, sizeof(XBAT_RES_HEADER) + dwFinalDataLen - 1);

	if (pCompressedBuffer) free(pCompressedBuffer);
	free(pHeader);
	return bRet;
}

static void ConverterProcess(XBAT_CONFIG* lpConfig, LPCTSTR szStubPath, LPCTSTR szOutputPath) {
	BOOL bEnableLzma = (lpConfig->GlobalFlags & XBAT_FLAG_LZMA_COMPRESSED);
	BYTE rawKey[16] = { 0 };
	XBat_GenerateRandomBytes(rawKey, 16);
	BYTE obfuscatedKey[16];
	for (int i = 0; i < 16; i++) {
		obfuscatedKey[i] = rawKey[i] ^ (XBAT_KEY_OBFUSCATOR + i);
	}

	DWORD dwScriptLen = 0;
	BYTE* pScriptBuffer = LoadFileToBuffer(g_szScriptPath, &dwScriptLen);
	if (!pScriptBuffer) return;

	if (!CopyFile(szStubPath, szOutputPath, FALSE)) {
		if (pScriptBuffer) free(pScriptBuffer);
		return;
	}

	HANDLE hUpdate = BeginUpdateResource(szOutputPath, FALSE);
	if (!hUpdate) return;
	UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_CONFIG), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), lpConfig, sizeof(*lpConfig));
	UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_KEY),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), obfuscatedKey, 16);
	PackAndInjectResourceEx(hUpdate, IDR_XBAT_BAT, pScriptBuffer, dwScriptLen, rawKey, NULL, 0, bEnableLzma);
	if (pScriptBuffer) free(pScriptBuffer);

	int nCurrentId = 501;
	for (size_t i = 0; i < g_ResList.size(); i++) {
		DWORD dwResLen = 0;
		BYTE* pRes = LoadFileToBuffer(g_ResList[i].szFilePath, &g_ResList[i].dwFileSize);

		if (pRes && nCurrentId <= 900) {
			PackAndInjectResourceEx(hUpdate, nCurrentId, pRes, g_ResList[i].dwFileSize, rawKey, g_ResList[i].szFilePath, g_ResList[i].dwFileAttribute, bEnableLzma);
		}
		nCurrentId++;
		if (pRes) free(pRes);
	}

	EndUpdateResource(hUpdate, FALSE);
}


int main(int argc, char* argv[]) {
#ifdef DEBUG
	_tprintf(_T("Debug mode\r\n"));
	XBAT_CONFIG cfg = { 0 };
	cfg.Magic = XBAT_MAGIC_INT;
	cfg.GlobalFlags = XBAT_FLAG_USE_PIPE | XBAT_FLAG_LZMA_COMPRESSED | XBAT_FLAG_DESTROY_RESOURCES | XBAT_FLAG_SELF_DESTROY;
	cfg.GlobalFlags |= XBAT_FLAG_SHOW_CONSOLE;
	cfg.GlobalFlags |= XBAT_FLAG_RUN_BAT_AS_FILE;
	cfg.GlobalFlags |= XBAT_FLAG_HAS_USER_RESOURCES;
	cfg.DropDirType = XBAT_DROP_DIR_TEMP;
	strcpy_s(cfg.szConsoleTitle, XBAT_CONSOLE_TITLE_LENGTH, "Test Stub");
	_tcscpy_s(g_szScriptPath, _countof(g_szScriptPath), _T("test_memory.bat"));

	XBAT_RESOURCE res1 = { 0 };
	_tcscpy_s(res1.szFilePath, MAX_PATH, _T("test1.jpg"));
	res1.dwFileSize = 0;
	res1.dwFileAttribute = FILE_ATTRIBUTE_NORMAL;
	g_ResList.push_back(res1);

	ConverterProcess(&cfg, _T("templates\\x64\\stub_full.bin"), _T("TestStub.exe"));
	ConverterProcess(&cfg, _T("templates\\x86\\stub_full.bin"), _T("TestStub_x86.exe"));
	_tprintf(_T("Done\r\n"));
#endif // DEBUG




	return 0;
}