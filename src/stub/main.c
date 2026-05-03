#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <time.h>
#include "../common/crypto.h"
#include "../common/shared_defs.h"

#ifdef MODE_FULL
#include "stub_full/stub_full.h"
#endif

HMODULE g_hStub;
BYTE FinalKey[XBAT_FINAL_KEY_LENGTH];
BYTE XBatMagic[] = XBAT_MAGIC_DATA;
XBAT_CONFIG g_Config;
TCHAR g_szSessionDropPath[MAX_PATH];
TCHAR g_szSessionResPath[MAX_PATH];
//TCHAR g_szBatWorkDir[MAX_PATH];

void XBat_GenerateRandomString(TCHAR *pszBuffer, DWORD dwSize);

BOOL DirectoryExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SetDefaultConfig(XBAT_CONFIG* Cfg){
	Cfg->Magic = *(UINT*)XBatMagic;
	Cfg->GlobalFlags = 0;
}

void InitGlobalConfig(HMODULE hMod){
	HRSRC hCfgRes = FindResource(hMod,MAKEINTRESOURCE(IDR_XBAT_CONFIG),(LPCTSTR)RT_RCDATA);
	if (hCfgRes){
		HGLOBAL hCfgData = LoadResource(hMod,hCfgRes);
		XBAT_CONFIG* pCfg = (XBAT_CONFIG*)LockResource(hCfgData);
		if (pCfg && pCfg->Magic == *(UINT*)XBatMagic){
			ZeroMemory(&g_Config,sizeof(g_Config));
			memcpy(&g_Config,pCfg,sizeof(XBAT_CONFIG));
			return;
		}
	}
	SetDefaultConfig(&g_Config);
}


// Free pContent manually is required if succeed
BYTE* XBat_ExtractResource(const LPVOID pData, DWORD dwSize, const BYTE* pKey, int keyLength, DWORD* pOutLen, TCHAR* pszOutFileName, DWORD* pdwOutAttrib){
	if (!pData || !pKey) return NULL;
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)pData;
	UINT Magic = pHeader->Magic;
	if (Magic != *(UINT*)XBatMagic) return NULL;
	
	if (pszOutFileName){
		_tcscpy_s(pszOutFileName, MAX_PATH, pHeader->szFileName);
	}
	
	if (pdwOutAttrib) {
		*pdwOutAttrib = pHeader->dwAttributes;
	}
	
	
	UINT SavedCrc = pHeader->SavedCrc;
	DWORD dwRawSize = pHeader->dwOriginalSize;
	
	
	BYTE* pContent = (BYTE*)malloc(dwRawSize);
	if (!pContent) return NULL;
	
	memcpy(pContent,pHeader->Data,dwRawSize);
	RC4_CTX ctx;
	RC4_Init(&ctx,pKey,keyLength);
	RC4_Process(&ctx,pContent,dwRawSize);
	
	if (CalculateCRC32(pContent,dwRawSize) != SavedCrc){
		free(pContent);
		return NULL;
		
	}
	
	if (pOutLen) *pOutLen = dwRawSize;
	return pContent;
	
}

#ifdef MODE_FULL
BYTE* XBat_ExtractResourceEx(const LPVOID pData, DWORD dwSize, const BYTE* pKey, int keyLength, DWORD* pOutLen, TCHAR* pszOutFileName, DWORD* pdwOutAttrib) {
	if (!pData || !pKey) return NULL;
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)pData;
	if (pHeader->Magic != *(UINT*)XBatMagic) return NULL;
	
	if (pszOutFileName) _tcscpy_s(pszOutFileName, MAX_PATH, pHeader->szFileName);
	if (pdwOutAttrib) *pdwOutAttrib = pHeader->dwAttributes;
	
	UINT SavedCrc = pHeader->SavedCrc;
	DWORD dwFinalSize = pHeader->dwOriginalSize;
	DWORD dwCompressedSize = dwSize - (sizeof(XBAT_RES_HEADER) - 1); 
	
	BYTE* pCompressed = (BYTE*)malloc(dwCompressedSize);
	if (!pCompressed) return NULL;
	
	memcpy(pCompressed, pHeader->Data, dwCompressedSize);
	RC4_CTX ctx;
	RC4_Init(&ctx, pKey, keyLength);
	RC4_Process(&ctx, pCompressed, dwCompressedSize);
	
	if (CalculateCRC32(pCompressed, dwCompressedSize) != SavedCrc) {
		free(pCompressed);
		return NULL;
	}
	
	BYTE* pFinal = XBat_DecompressBuffer(pCompressed, dwCompressedSize, dwFinalSize);
	
	free(pCompressed);
	
	if (pFinal && pOutLen) *pOutLen = dwFinalSize;
	return pFinal;
}
#endif



void InitResPath(){
	_stprintf_s(g_szSessionResPath, MAX_PATH, _T("%s\\Res"),g_szSessionDropPath);
	CreateDirectory(g_szSessionResPath, NULL);
	
}


BOOL CALLBACK EnumResNamesFunc(HMODULE hMod, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam){
	if (IS_INTRESOURCE(lpName)){
		HRSRC hRes = FindResource(hMod,lpName,(LPCTSTR)RT_RCDATA);
		DWORD dwSize = SizeofResource(hMod,hRes);
		HGLOBAL hData = LoadResource(hMod,hRes);
		void* pData = LockResource(hData);
		DWORD dwContentOutLen = 0;
		TCHAR szFileName[MAX_PATH];
		DWORD dwFileAttrib = 0;
		BYTE* pContent = NULL;
		
#ifdef MODE_FULL
		if (g_Config.GlobalFlags & XBAT_FLAG_LZMA_COMPRESSED){
			pContent = XBat_ExtractResourceEx(pData,dwSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwContentOutLen,szFileName,&dwFileAttrib);
		} else {
			pContent = XBat_ExtractResource(pData,dwSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwContentOutLen,szFileName,&dwFileAttrib);
		}
#else
		pContent = XBat_ExtractResource(pData,dwSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwContentOutLen,szFileName,&dwFileAttrib);
#endif
		
		if (pContent){
			
			
			TCHAR szFullPath[MAX_PATH];
			_stprintf_s(szFullPath, MAX_PATH, _T("%s\\%s"), g_szSessionResPath, szFileName);
			
			// Write file
			HANDLE hFile = CreateFile(szFullPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				DWORD dwWritten;
				WriteFile(hFile, pContent, dwContentOutLen, &dwWritten, NULL);
				CloseHandle(hFile);
				
				// Recover file attribute
				SetFileAttributes(szFullPath, dwFileAttrib);
			}
			
			
			free(pContent);
		}
		
	}
	
	return TRUE;
}

BOOL CALLBACK EnumResTypesFunc(HMODULE hMod, LPTSTR lpType, LONG_PTR lParam){
	if (IS_INTRESOURCE(lpType)){
		if ((UINT_PTR)lpType == (UINT_PTR)RT_RCDATA){
			EnumResourceNames(hMod,lpType,EnumResNamesFunc,0);
		}
	}
	return TRUE;
}

void RunBatAsFile(BYTE* pBatContent, DWORD dwContentLen)
{
	if (pBatContent == NULL || dwContentLen == 0) return;
	
	// Ensure directory exists
	if (!CreateDirectory(g_szSessionDropPath, NULL)){
		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_ALREADY_EXISTS){
			return;  // Failed
		}
	}
	
	TCHAR szRandom[5] = {0};
	XBat_GenerateRandomString(szRandom, 5);
	
	TCHAR szBatFullPath[MAX_PATH];
	_stprintf_s(szBatFullPath, MAX_PATH, _T("%s\\%s.bat"), g_szSessionDropPath, szRandom);

	// Write file (UTF-8 BOM)
	HANDLE hFile = CreateFile(
							  szBatFullPath,
							  GENERIC_WRITE,
							  0,
							  NULL,
							  CREATE_ALWAYS,
							  FILE_ATTRIBUTE_NORMAL,
							  NULL
							  );
	
	if (hFile == INVALID_HANDLE_VALUE) return;
	
	DWORD dwWritten = 0;
	
//	BYTE bBOM[] = { 0xEF, 0xBB, 0xBF };
//	WriteFile(hFile, bBOM, sizeof(bBOM), &dwWritten, NULL);
	
	WriteFile(hFile, pBatContent, dwContentLen, &dwWritten, NULL);
	
	CloseHandle(hFile);
	
	int sw;
	if (g_Config.GlobalFlags & XBAT_FLAG_SHOW_CONSOLE) sw = SW_SHOW;
	else sw = SW_HIDE;
	
	
	SHELLEXECUTEINFO sei = {0};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = _T("open");
	sei.lpFile = szBatFullPath;
	sei.nShow = sw;
	
	if (ShellExecuteEx(&sei)) {
		// Wait for process
		WaitForSingleObject(sei.hProcess, INFINITE);
		
		// Optional: Get exit code
//		DWORD dwExitCode = 0;
//		GetExitCodeProcess(sei.hProcess, &dwExitCode);
		
		CloseHandle(sei.hProcess);
	}
	
}

void ExecBat(BYTE* pBatContent, DWORD dwContentLen){

	if (g_Config.GlobalFlags & XBAT_FLAG_USE_PIPE) {
		if (g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE){
			// TODO: Fatih-like mode
			
		} else {
			// TODO: Full pipe mode
		}
		
	}
	
	if (g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE) {
		RunBatAsFile(pBatContent, dwContentLen);
	}
}

void StubProcess(){
	HMODULE hMod = g_hStub;
	
	// Retrieve key
	HRSRC hKeyRes = FindResource(hMod,MAKEINTRESOURCE(IDR_XBAT_KEY),(LPCTSTR)RT_RCDATA);
	HGLOBAL hKeyData = LoadResource(hMod,hKeyRes);
	BYTE* pKeyData = (BYTE*)LockResource(hKeyData);
	for (int i=0;i<16;i++){
		FinalKey[i] = pKeyData[i] ^ (XBAT_KEY_OBFUSCATOR + i);
	}
	
	// Retrieve script
	HRSRC hBatRes = FindResource(hMod,MAKEINTRESOURCE(IDR_XBAT_BAT),(LPCTSTR)RT_RCDATA);
	DWORD dwBatResSize = SizeofResource(hMod,hBatRes);
	HGLOBAL hBatData = LoadResource(hMod,hBatRes);
	void* pBatData = LockResource((hBatData));
	DWORD dwBatContentOutLen = 0;
	BYTE* pBatContent = NULL;
	

#ifdef MODE_FULL
	if (g_Config.GlobalFlags & XBAT_FLAG_LZMA_COMPRESSED){
		pBatContent = XBat_ExtractResourceEx(pBatData,dwBatResSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwBatContentOutLen,NULL,NULL);
	} else {
		pBatContent = XBat_ExtractResource(pBatData,dwBatResSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwBatContentOutLen,NULL,NULL);
	}
#else
	pBatContent = XBat_ExtractResource(pBatData,dwBatResSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwBatContentOutLen,NULL,NULL);
#endif
	
	
	if (pBatContent){
		//TODO: run bat in modes
		
		BYTE* pFinalContent = pBatContent;
		DWORD dwFinalLen =dwBatContentOutLen;
		
		// If has user resources
		if (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES){
			TCHAR szExePath[MAX_PATH];
			GetModuleFileName(NULL, szExePath, MAX_PATH);
			// Get the directory part
			TCHAR* pLastSlash = _tcsrchr(szExePath, _T('\\'));
			if (pLastSlash) *pLastSlash = _T('\0');
			// Build inject text
			TCHAR szInjectHeader[MAX_PATH * 3];
			_stprintf_s(szInjectHeader, _countof(szInjectHeader), _T("@chcp 65001 >nul\r\n@set RESDIR=%s\r\n@set EXEPATH=%s\r\n"), g_szSessionResPath, szExePath);
			
#ifdef UNICODE
			// Convert wide header in UNICODE to ANSI for script
			int nHeaderLen = WideCharToMultiByte(CP_UTF8, 0, szInjectHeader, -1, NULL, 0, NULL, NULL) - 1;
			char* pHeaderA = (char*)malloc(nHeaderLen + 1);
			WideCharToMultiByte(CP_UTF8, 0, szInjectHeader, -1, pHeaderA, nHeaderLen + 1, NULL, NULL);
#else
			int nHeaderLen = _tcslen(szInjectHeader);
			char* pHeaderA = szInjectHeader; // Point to ANSI header directly
#endif
			
			// Merge content
			dwFinalLen = nHeaderLen + dwBatContentOutLen;
			pFinalContent = (BYTE*)malloc(dwFinalLen);
			memcpy(pFinalContent, pHeaderA, nHeaderLen);
			memcpy(pFinalContent + nHeaderLen, pBatContent, dwBatContentOutLen);
			
#ifdef UNICODE
			free(pHeaderA); // Free the heap memory allocated due to W2MB conversion
#endif
			free(pBatContent); // Free the og bat content memory
			
			EnumResourceTypes(hMod,EnumResTypesFunc,0); // Parse user-added resources
		} 
		
		ExecBat(pFinalContent, dwFinalLen);
		
		if (pFinalContent) free(pFinalContent);
		
	} else {
		MessageBox(NULL,_T("Failed to load script"),_T("Error"),MB_ICONERROR);
	}
	
	
}


void StubDestroy(){
	if (DirectoryExists(g_szSessionDropPath)){
		SHFILEOPSTRUCT shfo;
		ZeroMemory(&shfo, sizeof(shfo));
		
		TCHAR szDelPath[MAX_PATH + 1];
		ZeroMemory(szDelPath, sizeof(szDelPath));
		
		_tcscpy_s(szDelPath, MAX_PATH, g_szSessionDropPath);
		
		shfo.pFrom = szDelPath;
		shfo.wFunc = FO_DELETE;
		shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;
		
		SHFileOperation(&shfo);
	}
	
	
}


void XBat_GenerateRandomString(TCHAR *pszBuffer, DWORD dwSize)
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

void InitSessionDropPath(){
	
	TCHAR szTempPath[MAX_PATH];
	
	switch (g_Config.DropDirType) {
	case XBAT_DROP_DIR_TEMP:
		if (GetTempPath(MAX_PATH, szTempPath) == 0){
			// Use current dir if failed
			GetCurrentDirectory(MAX_PATH, szTempPath);
		}
		break;
	case XBAT_DROP_DIR_CURR:
		GetCurrentDirectory(MAX_PATH, szTempPath);
		break;
	default:
		GetCurrentDirectory(MAX_PATH, szTempPath);
		break;
	}
	
	
	TCHAR szRandom[5] = {0};
	XBat_GenerateRandomString(szRandom, 5);
	_stprintf_s(g_szSessionDropPath, MAX_PATH, _T("%s\\XBAT.%s"), szTempPath, szRandom);
	
	// Create dir
	CreateDirectory(g_szSessionDropPath, NULL);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
	g_hStub = (HMODULE)hInstance;
	srand((unsigned int)time(NULL));
	
	InitGlobalConfig(g_hStub);
	
	if ((g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE) || (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES)){
		InitSessionDropPath();
		if (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES) InitResPath();
	}
	
	StubProcess();
	StubDestroy();
	
	
#ifdef MODE_FULL
	
#endif	
	
	return 0;
}


