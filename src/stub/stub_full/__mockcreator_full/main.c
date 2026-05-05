#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define ENABLE_LZMA
#define ENABLE_PIPE
//#define RUN_AS_FILE
#define SHOW_CONSOLE

#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#include "../../../common/lzma_sdk/C/LzmaEnc.h"
#include "../../../common/shared_defs.h"
#include "../../../common/crypto.h"

static void* SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void* address) { free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

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

BYTE* CompressDataLZMA(const BYTE* pRawData, DWORD dwRawLen, DWORD* pOutCompressedLen) {
	// 1. 预估输出缓冲区大小
	SizeT destLen = (SizeT)dwRawLen + dwRawLen / 8 + 65536;
	BYTE* pDestBuf = (BYTE*)malloc(destLen);
	if (!pDestBuf) return NULL;
	
	// 2. 设置压缩参数
	CLzmaEncProps props;
	LzmaEncProps_Init(&props);
	props.level = 9;            // 最高压缩率
	props.dictSize = 1 << 24;   // 16MB 字典
	props.writeEndMark = 0;     // 不写入结束符（我们靠长度控制）
	
	// 3. 准备存放 5 字节 Props 的空间
	// 我们把 Props 存放在 pDestBuf 的开头，压缩数据紧随其后
	SizeT propsSize = LZMA_PROPS_SIZE;
	BYTE propsEncoded[LZMA_PROPS_SIZE];
	
	// 指向压缩数据存放的起始位置（跳过 5 字节位置，后面再合并）
	// 或者更简单的做法：直接先压到另一个位置，再拷贝。
	// 这里为了效率，直接预留头部。
	SizeT outSizeProcessed = destLen - LZMA_PROPS_SIZE;
	
	// 4. 调用官方推荐的 RAM->RAM 接口
	SRes res = LzmaEncode(
						  pDestBuf + LZMA_PROPS_SIZE, &outSizeProcessed, // 压缩数据目标
						  pRawData, (SizeT)dwRawLen,                      // 源数据
						  &props,                                         // 参数
						  pDestBuf, &propsSize,                           // Props 存放位置（直接存到缓冲区开头）
						  props.writeEndMark,
						  NULL,                                           // progress callback
						  &g_Alloc, &g_Alloc                              // 分配器
						  );
	
	if (res != SZ_OK) {
		free(pDestBuf);
		return NULL;
	}
	
	// 总长度 = 5 字节 Props + 实际压缩出来的数据长度
	*pOutCompressedLen = (DWORD)(outSizeProcessed + LZMA_PROPS_SIZE);
	return pDestBuf;
}

BOOL PackAndInjectResourceEx(
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
		_tcscpy_s(pHeader->szFileName, MAX_PATH, szTargetFileName);
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

void CreateTestStub(LPCTSTR szStubPath, LPCTSTR szOutputPath) {
	XBAT_CONFIG cfg = {0};
	cfg.Magic = XBAT_MAGIC_INT;
	cfg.GlobalFlags = XBAT_FLAG_HAS_USER_RESOURCES;
	cfg.DropDirType = XBAT_DROP_DIR_TEMP;
	strcpy(cfg.szConsoleTitle, "Test Stub");

	BOOL bEnableLzma = FALSE;
	
#ifdef ENABLE_LZMA
	cfg.GlobalFlags |= XBAT_FLAG_LZMA_COMPRESSED;
	bEnableLzma = TRUE;
#endif
	
#ifdef ENABLE_PIPE
	cfg.GlobalFlags |= XBAT_FLAG_USE_PIPE;
#endif
	
#ifdef SHOW_CONSOLE
	cfg.GlobalFlags |= XBAT_FLAG_SHOW_CONSOLE;
#endif
	
#ifdef RUN_AS_FILE
	cfg.GlobalFlags |= XBAT_FLAG_RUN_BAT_AS_FILE;
#endif

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
		if (pRes1) free(pRes1); if (pRes2) free(pRes2);
		return;
	}
	
	HANDLE hUpdate = BeginUpdateResource(szOutputPath, FALSE);
	if (hUpdate) {
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_CONFIG),
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &cfg, sizeof(cfg));
		
		UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_KEY),
					   MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), obfuscatedKey, 16);
		
		PackAndInjectResourceEx(hUpdate, IDR_XBAT_BAT, pScriptBuffer, dwScriptLen, rawKey, NULL, 0, bEnableLzma);
		
		int nCurrentID = 501;
		if (pRes1 && nCurrentID <= 900) {
			PackAndInjectResourceEx(hUpdate, nCurrentID++, pRes1, dwRes1Len, rawKey, _T("test1.jpg"), FILE_ATTRIBUTE_HIDDEN, bEnableLzma);
		}
		if (pRes2 && nCurrentID <= 900) {
			PackAndInjectResourceEx(hUpdate, nCurrentID++, pRes2, dwRes2Len, rawKey, _T("test2.exe"), FILE_ATTRIBUTE_NORMAL, bEnableLzma);
		}
		
		EndUpdateResource(hUpdate, FALSE);
	}
	
	if (pScriptBuffer) free(pScriptBuffer);
	if (pRes1) free(pRes1);
	if (pRes2) free(pRes2);
	
	_tprintf(_T("Created: %s (LZMA: %s)\n"), szOutputPath, bEnableLzma ? _T("ON") : _T("OFF"));
}

int main() {
	CreateTestStub(_T("stub_full.exe"), _T("TestStub.exe"));
	return 0;
}
