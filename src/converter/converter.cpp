#include <stdio.h>
#include <Windows.h>
#include <tchar.h>
#include <time.h>
#include <vector>
#include <string>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

// MODE_CONVERTER SHOULD BE DEFINED GLOBALLY FOR THE PROJECT IN IDE
#ifndef MODE_CONVERTER
#define MODE_CONVERTER
#endif // !MODE_CONVERTER

#ifdef __cplusplus
extern "C" {
#endif
#include "../common/shared_defs.h"
#include "../common/lzma_sdk/C/LzmaEnc.h"
#include "../common/crypto.h"
#include "../common/Utils.h"
#include "restool.h"
#ifdef __cplusplus
}
#endif

#define PARSE_STR_PARAM(pattern, dest, errMsg) \
    if (IsArgEqual(lpszArg, pattern)) { \
        if (i + 1 < argc) { \
            strncpy_s(dest, argv[++i], _TRUNCATE); \
        } else { \
            fprintf(stderr, "Error: %s\n", errMsg); \
            return FALSE; \
        } \
    }

#define PARSE_INT_PARAM(pattern, dest, errMsg) \
    if (IsArgEqual(lpszArg, pattern)) { \
        if (i + 1 < argc) { \
            dest = atoi(argv[++i]); \
        } else { \
            fprintf(stderr, "Error: %s\n", errMsg); \
            return FALSE; \
        } \
    }

#define CheckArgPattern(pattern) IsArgEqual(lpszArg, pattern)

TCHAR g_szGoRCPath[MAX_PATH];


bool IsArgEqual(const char* lpszArg, const char* lpszPattern) {
	if (!lpszArg || !lpszPattern) return false;
	while (*lpszArg && *lpszPattern) {
		char c1 = *lpszArg;
		char c2 = *lpszPattern;
		if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
		if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
		if (c1 != c2) return false;
		lpszArg++;
		lpszPattern++;
	}
	return *lpszArg == *lpszPattern;
}

typedef struct {
	CHAR szSrcBatPath[MAX_PATH];
	CHAR szTargetExePath[MAX_PATH];
	std::vector<std::string>vecIncPaths;
	BOOL bShowConsole;
	XBAT_MODE eMode;
	BOOL bEnableLzma;
	BOOL bHasUserRes;
	BOOL bDestroyRes;
	BOOL bUseX64;
	int nDropDirType;
	CHAR szVerInfoPath[MAX_PATH]; // Accept stdin input of file if use "-"
	CHAR szIconPath[MAX_PATH];
} CONVERTER_OPTIONS;

typedef struct {
	TCHAR szFilePath[MAX_PATH];
	DWORD dwFileSize; // Determined in ConverterProcess
	DWORD dwFileAttribute;

} XBAT_RESOURCE;

typedef struct {
	XBAT_CONFIG* lpConfig;
	std::vector<XBAT_RESOURCE> vecResList; // User-added resources
	TCHAR szVerInfoPath[MAX_PATH]; // User-provided ini or temp ini path
	TCHAR szIconPath[MAX_PATH];
	TCHAR szScriptPath[MAX_PATH];
	TCHAR szStubPath[MAX_PATH];
	TCHAR szOutputPath[MAX_PATH];
} CONVERTER_LIST;

static void* SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void* address) { free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

static LPBYTE LoadFileToBuffer(LPCTSTR lpszFilePath, LPDWORD lpdwSize) {
	HANDLE hFile = CreateFile(lpszFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return NULL;

	*lpdwSize = GetFileSize(hFile, NULL);
	BYTE* pBuffer = (BYTE*)malloc(*lpdwSize);
	if (pBuffer) {
		DWORD dwRead;
		(void)ReadFile(hFile, pBuffer, *lpdwSize, &dwRead, NULL);
	}
	CloseHandle(hFile);
	return pBuffer;
}

static LPBYTE CompressDataLZMA(const LPBYTE lpRawData, DWORD dwRawLen, LPDWORD lpOutCompressedLen) {
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
		lpRawData, (SizeT)dwRawLen,
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

	*lpOutCompressedLen = (DWORD)(outSizeProcessed + LZMA_PROPS_SIZE);
	return pDestBuf;
}

static BOOL PackAndInjectResourceEx(
	HANDLE hUpdate,
	int ResourceID,
	const LPBYTE lpRawData,
	DWORD dwDataLen,
	const LPBYTE lpKey,
	LPCTSTR lpszTargetFileName,
	DWORD dwAttrib,
	BOOL bCompress
) {
	BYTE* pFinalDataToEncrypt = (BYTE*)lpRawData;
	DWORD dwFinalDataLen = dwDataLen;
	BYTE* pCompressedBuffer = NULL;


	if (bCompress) {
		pCompressedBuffer = CompressDataLZMA(lpRawData, dwDataLen, &dwFinalDataLen);
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
	if (lpszTargetFileName) {
		_tcscpy_s(pHeader->szFileName, XBAT_RES_FILE_NAME_LENGTH, lpszTargetFileName);
	}

	memcpy(pHeader->Data, pFinalDataToEncrypt, dwFinalDataLen);
	RC4_CTX ctx;
	RC4_Init(&ctx, lpKey, 16);
	RC4_Process(&ctx, pHeader->Data, dwFinalDataLen);

	BOOL bRet = UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(ResourceID),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		pHeader, sizeof(XBAT_RES_HEADER) + dwFinalDataLen - 1);

	if (pCompressedBuffer) free(pCompressedBuffer);
	free(pHeader);
	return bRet;
}

static void ConverterProcess(CONVERTER_LIST* lpList) {
	BOOL bEnableLzma = (lpList->lpConfig->GlobalFlags & XBAT_FLAG_LZMA_COMPRESSED);
	BYTE rawKey[16] = { 0 }; // The key to encrypt resources
	XBat_GenerateRandomBytes(rawKey, 16);
	BYTE obfuscatedKey[16];
	for (int i = 0; i < 16; i++) {
		obfuscatedKey[i] = rawKey[i] ^ (XBAT_KEY_OBFUSCATOR + i);
	}

	DWORD dwScriptLen = 0;
	BYTE* pScriptBuffer = LoadFileToBuffer(lpList->szScriptPath, &dwScriptLen);
	if (!pScriptBuffer) return;

	if (!CopyFile(lpList->szStubPath, lpList->szOutputPath, FALSE)) {
		if (pScriptBuffer) free(pScriptBuffer);
		return;
	}

	HANDLE hUpdate = BeginUpdateResource(lpList->szOutputPath, FALSE);
	if (!hUpdate) return;
	UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_CONFIG), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), lpList->lpConfig, sizeof(*(lpList->lpConfig)));
	UpdateResource(hUpdate, RT_RCDATA, MAKEINTRESOURCE(IDR_XBAT_KEY),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), obfuscatedKey, 16);
	PackAndInjectResourceEx(hUpdate, IDR_XBAT_BAT, pScriptBuffer, dwScriptLen, rawKey, NULL, 0, bEnableLzma);
	if (pScriptBuffer) free(pScriptBuffer);

	int nCurrentId = 501;
	for (size_t i = 0; i < lpList->vecResList.size(); i++) {
		DWORD dwResLen = 0;
		BYTE* pRes = LoadFileToBuffer(lpList->vecResList[i].szFilePath, &(lpList->vecResList[i].dwFileSize));

		if (pRes && nCurrentId <= 900) {
			PackAndInjectResourceEx(hUpdate, nCurrentId, pRes, lpList->vecResList[i].dwFileSize, rawKey, lpList->vecResList[i].szFilePath, lpList->vecResList[i].dwFileAttribute, bEnableLzma);
		}
		nCurrentId++;
		if (pRes) free(pRes);
	}

	EndUpdateResource(hUpdate, FALSE);

	// Add icon and version info

	STUB_VERSION_INFO svi = { 0 };
	BOOL bHasVersionInfo = FALSE;

	if (lpList->szVerInfoPath[0] != _T('\0')) {
		bHasVersionInfo = TRUE;
		// Default values
		WORD wYear = 2026;
		SYSTEMTIME st = { 0 };
		GetLocalTime(&st);
		wYear = st.wYear;
		_tcscpy_s(svi.szComments, 128, _T("Packed by XBat Converter."));
		_tcscpy_s(svi.szCompanyName, 128, _T("XBat Project"));
		_tcscpy_s(svi.szFileDescription, 128, _T("XBat Packed Executable Application"));
		_tcscpy_s(svi.szFileVersion, 128, _T("1.0.0.0"));
		_tcscpy_s(svi.szInternalName, 128, _T("XBatStub.exe"));
		_stprintf_s(svi.szLegalCopyright, 128, _T("Copyright (C) %d. All rights reserved."), wYear);
		_tcscpy_s(svi.szLegalTrademarks, 128, _T("XBat(TM)"));
		_tcscpy_s(svi.szOriginalFilename, 128, _T("XBatOutput.exe"));
		_tcscpy_s(svi.szPrivateBuild, 128, _T(""));
		_tcscpy_s(svi.szProductName, 128, _T("XBat Generated Application"));
		_tcscpy_s(svi.szProductVersion, 128, _T("1.0.0.0"));
		_tcscpy_s(svi.szSpecialBuild, 128, _T(""));

		// TODO: Extract from ini and override

	}

	TCHAR szResPath[MAX_PATH];
	DWORD dwResPathLen = 0;
	
	if (!BuildResourceFile(lpList->szIconPath, &svi, szResPath, dwResPathLen, bHasVersionInfo)) return;

	InjectResIntoExe(lpList->szStubPath, szResPath);

}


static BOOL ParseCmdLine(int argc, char* argv[], CONVERTER_OPTIONS* lpOpt) {
	ZeroMemory(lpOpt, sizeof(CONVERTER_OPTIONS));

	// Default behavior
	lpOpt->bShowConsole = TRUE;
	lpOpt->eMode = MODE_FATIH;
	lpOpt->bDestroyRes = TRUE;
	lpOpt->bEnableLzma = FALSE;
	lpOpt->bHasUserRes = FALSE;
	lpOpt->nDropDirType = XBAT_DROP_DIR_TEMP;

	if (argc < 2) return FALSE;

	for (int i = 1; i < argc; ++i) {
		const char* lpszArg = argv[i];

		PARSE_STR_PARAM("/bat", lpOpt->szSrcBatPath, "/bat requires a file path")
		else PARSE_STR_PARAM("/exe", lpOpt->szTargetExePath, "/exe requires an output path.")
		else PARSE_STR_PARAM("/icon", lpOpt->szIconPath, "/icon requires a file path.")
		else PARSE_STR_PARAM("/ver", lpOpt->szVerInfoPath, "/ver requires a file path or '-'.")

		else if (IsArgEqual(lpszArg, "/include")) {
			if (i + 1 < argc) {
				//strncpy_s(lpOpt->szIncDirPath, argv[++i], _TRUNCATE);
				lpOpt->vecIncPaths.push_back(argv[++i]);
				lpOpt->bHasUserRes = TRUE;
			}
			else {
				fprintf(stderr, "Error: /include requires a file or directory path.\n");
				return FALSE;
			}
		}

		else PARSE_INT_PARAM("/dropdir", lpOpt->nDropDirType, "/dropdir requires an integer value.")

		else if (IsArgEqual(lpszArg, "/mode")) {
			if (i + 1 < argc) {
				const char* lpszMode = argv[++i];
				if (IsArgEqual(lpszMode, "memory")) {
					lpOpt->eMode = MODE_MEMORY;
				}
				else if (IsArgEqual(lpszMode, "lite")) {
					lpOpt->eMode = MODE_LITE;
				}
				else if (IsArgEqual(lpszMode, "standard")) {
					lpOpt->eMode = MODE_FATIH;
				}
				else {
					fprintf(stderr, "Error: Unknown mode '%s'. Integrated modes: standard, memory, lite.\n", lpszMode);
					return FALSE;
				}
			}
			else {
				fprintf(stderr, "Error: /mode requires a type (standard|memory|lite).\n");
				return FALSE;
			}
		}

		else if (CheckArgPattern("/invisible")) {
			lpOpt->bShowConsole = FALSE;
		}
		else if (CheckArgPattern("/lzma")) {
			lpOpt->bEnableLzma = TRUE;
		}
		else if (CheckArgPattern("/noclean")) {
			lpOpt->bDestroyRes = FALSE;
		}
		else if (CheckArgPattern("/x64")) {
			lpOpt->bUseX64 = TRUE;
		}
	}

	if (lpOpt->szSrcBatPath[0] == '\0') {
		fprintf(stderr, "Error: Source batch file (/bat) is required.\n\n");
		return FALSE;
	}
	
	
	return TRUE;
}



int main(int argc, char* argv[]) {
	
	CONVERTER_OPTIONS opt = { 0 };
	XBAT_CONFIG cfg = { 0 };
	CONVERTER_LIST lst = { 0 };

	if (!ParseCmdLine(argc, argv, &opt)) return FALSE;
	
	// Default config values
	cfg.GlobalFlags |= XBAT_FLAG_SELF_DESTROY;
	cfg.DropDirType = XBAT_DROP_DIR_TEMP;

	if (opt.szTargetExePath[0] == '\0') {
		strcpy_s(opt.szTargetExePath, MAX_PATH, opt.szSrcBatPath);
		if (!PathRenameExtensionA(opt.szTargetExePath, ".exe")) {
			fprintf(stderr, "Error: Failed to change extension.\n");
		}
	}

	if (opt.bShowConsole) cfg.GlobalFlags |= XBAT_FLAG_SHOW_CONSOLE;
	switch (opt.eMode) {
	case MODE_FATIH:
		cfg.GlobalFlags |= XBAT_FLAG_USE_PIPE;
		cfg.GlobalFlags |= XBAT_FLAG_RUN_BAT_AS_FILE;
		break;
	case MODE_LITE:
		cfg.GlobalFlags |= XBAT_FLAG_RUN_BAT_AS_FILE;
		break;
	case MODE_MEMORY:
		cfg.GlobalFlags |= XBAT_FLAG_USE_PIPE;
		break;
	}

	if (opt.bEnableLzma) cfg.GlobalFlags |= XBAT_FLAG_LZMA_COMPRESSED;
	if (opt.bDestroyRes) cfg.GlobalFlags |= XBAT_FLAG_DESTROY_RESOURCES;
	if (opt.nDropDirType >= 0) {
		cfg.DropDirType = (UINT)opt.nDropDirType;
	}
	else {
		fprintf(stderr, "Error: Undefined drop directory type.\n");
	}

	// Parse user resources
	if (opt.bHasUserRes) {
		cfg.GlobalFlags |= XBAT_FLAG_HAS_USER_RESOURCES;
		
		for (size_t i = 0; i < opt.vecIncPaths.size(); ++i) {
			const char* pszPath = opt.vecIncPaths[i].c_str();
			DWORD dwAttr = GetFileAttributesA(pszPath);
			if (dwAttr == INVALID_FILE_ATTRIBUTES) continue;
			if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
				// TODO: use 7z-sfx to pack
			}
			else {
				// Pack single file
				XBAT_RESOURCE res = { 0 };
				CHAR2TCHAR(res.szFilePath, opt.vecIncPaths[i].data(), MAX_PATH);
				res.dwFileAttribute = GetFileAttributes(res.szFilePath);
				lst.vecResList.push_back(res);

			}
		}
	}

	if (opt.szVerInfoPath[0] != '\0') {
		if (opt.szVerInfoPath[0] == '-') {
			// TODO: Parse stdin and write to temp file
			HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
			if (hStdin == INVALID_HANDLE_VALUE) {
				fprintf(stderr, "Error: Failed to obtain stdin.\n");
				return FALSE;
			}

			TCHAR szTempPath[MAX_PATH];
			TCHAR szTempFileName[MAX_PATH];
			GetTempPath(MAX_PATH, szTempPath);
			GetTempFileName(szTempPath, _T("XBat_Ver_"), 0, szTempFileName);

			HANDLE hTempFile = CreateFile(szTempFileName, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hTempFile == INVALID_HANDLE_VALUE) {
				return FALSE;
			}

			BYTE buffer[STDIN_BUF_SIZE];
			DWORD dwBytesRead = 0;
			DWORD dwBytesWritten = 0;

			while (ReadFile(hStdin, buffer, STDIN_BUF_SIZE, &dwBytesRead, NULL) && dwBytesRead > 0) {
				WriteFile(hTempFile, buffer, dwBytesRead, &dwBytesWritten, NULL);
			}

			CloseHandle(hTempFile);

			_tcscpy_s(lst.szVerInfoPath, MAX_PATH, szTempFileName);

			// 【注意】记得在 XBat 编译生成 EXE 功成身退的最后，
			// 调用 DeleteFile(lst.szVerInfoPath) 擦除这个临时文件。
		}
		else {
			// Copy path directly
			CHAR2TCHAR(lst.szVerInfoPath, opt.szVerInfoPath, MAX_PATH);
		}
	}
	
	if (opt.szIconPath[0] != '\0') {
		CHAR2TCHAR(lst.szIconPath, opt.szIconPath, MAX_PATH);
	}

	lst.lpConfig = &cfg;

	if (opt.szSrcBatPath[0] != '\0') {
		CHAR2TCHAR(lst.szScriptPath, opt.szSrcBatPath, MAX_PATH);
	}

	if (opt.szTargetExePath[0] != '\0') {
		CHAR2TCHAR(lst.szOutputPath, opt.szTargetExePath, MAX_PATH);
	}

	// Construct stub path
	_tcscpy_s(lst.szStubPath, MAX_PATH, _T("templates\\"));
	if (opt.bUseX64) {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("x64\\"));
	}
	else {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("x86\\"));
	}
	if (opt.eMode == MODE_LITE) {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("stub_lite.bin"));
	}
	else {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("stub_full.bin"));
	}

	// Path fields
	CHAR2TCHAR(lst.szScriptPath, opt.szSrcBatPath, MAX_PATH);
	CHAR2TCHAR(lst.szOutputPath, opt.szTargetExePath, MAX_PATH);

	ConverterProcess(&lst);

	return TRUE;
}