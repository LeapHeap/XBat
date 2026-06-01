#include <stdio.h>
#include <Windows.h>
#include <tchar.h>
#include <vector>
#include <string>

// MODE_CONVERTER SHOULD BE DEFINED GLOBALLY FOR THE PROJECT IN IDE
#ifndef MODE_CONVERTER
#define MODE_CONVERTER
#endif // !MODE_CONVERTER

#ifdef __cplusplus
extern "C" {
#endif
#include "../common/shared_defs.h"
#include "../common/crypto.h"
#include "../common/Utils.h"
#include "../common/version.h"
#ifdef __cplusplus
}
#endif

#include "converter.h"

TCHAR g_szTempWorkDirPath[MAX_PATH] = { 0 };
TCHAR g_szTempArchivePath[MAX_PATH] = { 0 };
TCHAR g_szConverterExePath[MAX_PATH] = { 0 };
TCHAR g_szConverterDirPath[MAX_PATH] = { 0 };

BOOL DirectoryExists(LPCTSTR lpszPath)
{
	DWORD dwAttrib = GetFileAttributes(lpszPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL InitTempWorkDir() {
	TCHAR szTempPath[MAX_PATH];
	if (GetTempPath(MAX_PATH, szTempPath) == 0) {
		// Use current dir if failed
		GetCurrentDirectory(MAX_PATH, szTempPath);
	}
	TCHAR szRandom[5] = { 0 };
	XBat_GenerateRandomString(szRandom, 5);
	_stprintf_s(g_szTempWorkDirPath, MAX_PATH, _T("%s\\XC.%s"), szTempPath, szRandom);

	return CreateDirectory(g_szTempWorkDirPath, NULL);
}

int DestroyTempWorkDir() {
	if (!DirectoryExists(g_szTempWorkDirPath)) return FALSE;

	SHFILEOPSTRUCT shfo;
	ZeroMemory(&shfo, sizeof(shfo));

	shfo.pFrom = g_szTempWorkDirPath;
	shfo.wFunc = FO_DELETE;
	// FOF_NOERRORUI:
	shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

	return SHFileOperation(&shfo);
}

LPBYTE LoadFileToBuffer(LPCTSTR lpszFilePath, LPDWORD lpdwSize) {
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


BOOL PackAndInjectResourceEx(
	HANDLE hUpdate,
	int ResourceID,
	const LPBYTE lpRawData,
	DWORD dwDataLen,
	const LPBYTE lpKey,
	LPCTSTR lpszTargetFileName,
	DWORD dwAttrib
) {
	BYTE* pFinalDataToEncrypt = (BYTE*)lpRawData;
	DWORD dwFinalDataLen = dwDataLen;

	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)malloc(sizeof(XBAT_RES_HEADER) + dwFinalDataLen);
	if (!pHeader) {
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

	free(pHeader);
	return bRet;
}

void ConverterProcess(CONVERTER_LIST* lpList) {
	// Using FALSE since lzma is deprecated
	BOOL bEnableLzma = FALSE;
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

	// Original logic
	PackAndInjectResourceEx(hUpdate, IDR_XBAT_BAT, pScriptBuffer, dwScriptLen, rawKey, NULL, 0);

	// Free original script buffer anyway
	if (pScriptBuffer) free(pScriptBuffer);

	// Walk and add user resources
	int nCurrentId = IDR_XBAT_USER_RES_START;
	for (size_t i = 0; i < lpList->vecResList.size(); i++) {
		DWORD dwResLen = 0;
		BYTE* pRes = LoadFileToBuffer(lpList->vecResList[i].szFilePath, &(lpList->vecResList[i].dwFileSize));

		if (pRes && nCurrentId <= IDR_XBAT_USER_RES_END) {
			// Get the pointer and length of current full path
			const TCHAR* pszFullPath = lpList->vecResList[i].szFilePath;
			int p = (int)_tcslen(pszFullPath);

			// Extract pure file name
			while (p > 0 && pszFullPath[p - 1] != _T('\\') && pszFullPath[p - 1] != _T('/')) {
				p--;
			}
			const TCHAR* pszPureFileName = pszFullPath + p;

			// Inject current resource
			PackAndInjectResourceEx(
				hUpdate,
				nCurrentId,
				pRes,
				lpList->vecResList[i].dwFileSize,
				rawKey,
				pszPureFileName,
				lpList->vecResList[i].dwFileAttribute
			);
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
		// ini path is lpList->szVerInfoPath
		LPCTSTR lpszIniSection = _T("VersionInfo");
		TCHAR szAbsoluteIniPath[MAX_PATH];
		if (GetFullPathName(lpList->szVerInfoPath, MAX_PATH, szAbsoluteIniPath, NULL) == 0) {
			_tcscpy_s(szAbsoluteIniPath, MAX_PATH, lpList->szVerInfoPath);
		}
		LPCTSTR lpszIniPath = szAbsoluteIniPath;

		TCHAR szValue[128];

#pragma region INI_FIELD_EXTRACT_AND_OVERRIDE

		// 1. Comments
		if (GetPrivateProfileString(lpszIniSection, _T("Comments"), svi.szComments, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szComments, 128, szValue);
		}
		// 2. CompanyName
		if (GetPrivateProfileString(lpszIniSection, _T("CompanyName"), svi.szCompanyName, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szCompanyName, 128, szValue);
		}
		// 3. FileDescription
		if (GetPrivateProfileString(lpszIniSection, _T("FileDescription"), svi.szFileDescription, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szFileDescription, 128, szValue);
		}
		// 4. FileVersion
		if (GetPrivateProfileString(lpszIniSection, _T("FileVersion"), svi.szFileVersion, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szFileVersion, 128, szValue);
		}
		// 5. InternalName
		if (GetPrivateProfileString(lpszIniSection, _T("InternalName"), svi.szInternalName, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szInternalName, 128, szValue);
		}
		// 6. LegalCopyright
		if (GetPrivateProfileString(lpszIniSection, _T("LegalCopyright"), svi.szLegalCopyright, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szLegalCopyright, 128, szValue);
		}
		// 7. LegalTrademarks
		if (GetPrivateProfileString(lpszIniSection, _T("LegalTrademarks"), svi.szLegalTrademarks, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szLegalTrademarks, 128, szValue);
		}
		// 8. OriginalFilename
		if (GetPrivateProfileString(lpszIniSection, _T("OriginalFilename"), svi.szOriginalFilename, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szOriginalFilename, 128, szValue);
		}
		// 9. PrivateBuild
		if (GetPrivateProfileString(lpszIniSection, _T("PrivateBuild"), svi.szPrivateBuild, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szPrivateBuild, 128, szValue);
		}
		// 10. ProductName
		if (GetPrivateProfileString(lpszIniSection, _T("ProductName"), svi.szProductName, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szProductName, 128, szValue);
		}
		// 11. ProductVersion
		if (GetPrivateProfileString(lpszIniSection, _T("ProductVersion"), svi.szProductVersion, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szProductVersion, 128, szValue);
		}
		// 12. SpecialBuild
		if (GetPrivateProfileString(lpszIniSection, _T("SpecialBuild"), svi.szSpecialBuild, szValue, 128, lpszIniPath) > 0) {
			_tcscpy_s(svi.szSpecialBuild, 128, szValue);
		}

#pragma endregion

	}

	TCHAR szDestExePath[MAX_PATH];
	_tcscpy_s(szDestExePath, MAX_PATH, lpList->szOutputPath);

	TCHAR szRceditPath[MAX_PATH];
	_stprintf_s(szRceditPath, MAX_PATH, _T("%s\\tools\\rcedit.exe"), g_szConverterDirPath);

	static TCHAR szRcCmdLine[MAX_PATH * 15];
	_stprintf_s(szRcCmdLine, _countof(szRcCmdLine), _T("\"%s\" \"%s\""), szRceditPath, szDestExePath);

	_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine),
		_T(" --set-file-version \"%s\" --set-product-version \"%s\""), svi.szFileVersion, svi.szProductVersion);

	if (_tcslen(svi.szCompanyName) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"CompanyName\" \"%s\""), svi.szCompanyName);
	}
	if (_tcslen(svi.szFileDescription) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"FileDescription\" \"%s\""), svi.szFileDescription);
	}
	if (_tcslen(svi.szInternalName) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"InternalName\" \"%s\""), svi.szInternalName);
	}
	if (_tcslen(svi.szLegalCopyright) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"LegalCopyright\" \"%s\""), svi.szLegalCopyright);
	}
	if (_tcslen(svi.szOriginalFilename) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"OriginalFilename\" \"%s\""), svi.szOriginalFilename);
	}
	if (_tcslen(svi.szProductName) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"ProductName\" \"%s\""), svi.szProductName);
	}
	if (_tcslen(svi.szComments) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"Comments\" \"%s\""), svi.szComments);
	}
	if (_tcslen(svi.szLegalTrademarks) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"LegalTrademarks\" \"%s\""), svi.szLegalTrademarks);
	}
	if (_tcslen(svi.szPrivateBuild) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"PrivateBuild\" \"%s\""), svi.szPrivateBuild);
	}
	if (_tcslen(svi.szSpecialBuild) > 0) {
		_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine), _T(" --set-version-string \"SpecialBuild\" \"%s\""), svi.szSpecialBuild);
	}

	if (lpList->szIconPath[0] != _T('\0')) {
		DWORD dwIconAttr = GetFileAttributes(lpList->szIconPath);
		if (dwIconAttr != INVALID_FILE_ATTRIBUTES && !(dwIconAttr & FILE_ATTRIBUTE_DIRECTORY)) {
			_stprintf_s(szRcCmdLine + _tcslen(szRcCmdLine), _countof(szRcCmdLine) - _tcslen(szRcCmdLine),
				_T(" --set-icon \"%s\""), lpList->szIconPath);
		}
	}

	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	if (CreateProcess(
		NULL,
		szRcCmdLine,
		NULL,
		NULL,
		FALSE,
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	else {
		fprintf(stderr, "Error: Failed to execute rcedit.exe to inject resources.\n");
	}

}
