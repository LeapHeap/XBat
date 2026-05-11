#ifndef MODE_VC6
#ifndef MODE_FULL
#define MODE_FULL
#endif
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#endif

#include <windows.h>
#include <tchar.h>
#include <time.h>
#include "../common/shared_defs.h"
#include "../common/crypto.h"
#include "../common/Utils.h"

#include "../common/vc6_patch.h"

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

char* WCharToUtf8(const WCHAR* wideStr) {
	if (wideStr == NULL) return NULL;
	int needSize = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, NULL, 0, NULL, NULL);
	if (needSize <= 0) return NULL;
	
	char* multiStr = (char*)malloc(needSize);
	if (multiStr == NULL) return NULL;
	
	WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, multiStr, needSize, NULL, NULL);
	return multiStr;
}

// WARNING: Manual free is required after use
char* WCharToAnsi(const WCHAR* wideStr) {
	if (wideStr == NULL) return NULL;
	
	int needSize = WideCharToMultiByte(CP_ACP, 0, wideStr, -1,
									   NULL, 0, NULL, NULL);
	if (needSize <= 0) return NULL;
	
	char* multiStr = (char*)malloc(needSize);
	if (multiStr == NULL) return NULL;
	
	int result = WideCharToMultiByte(CP_ACP, 0, wideStr, -1,
									 multiStr, needSize, NULL, NULL);
	if (result == 0) {
		free(multiStr);
		return NULL;
	}
	
	return multiStr;
}

static void ShowErrorMessage(LPCTSTR lpszMsg) {
	DWORD dwErr = GetLastError();
	TCHAR szBuf[256];
	_sntprintf(szBuf, _countof(szBuf), _T("%s. Error: %u (0x%08X)"), lpszMsg, dwErr, dwErr);
	szBuf[_countof(szBuf)-1] = _T('\0');
	MessageBox(NULL, szBuf, _T("Stub Error"), MB_ICONERROR);
}

BOOL DirectoryExists(LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SetDefaultConfig(XBAT_CONFIG* Cfg){
	Cfg->Magic = *(UINT*)XBatMagic;
	Cfg->GlobalFlags = 0;
	Cfg->DropDirType = XBAT_DROP_DIR_TEMP;
	strcpy(Cfg->szConsoleTitle, "XBat Executor Console");
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
		_tcsncpy(pszOutFileName, pHeader->szFileName, XBAT_RES_FILE_NAME_LENGTH - 1);
		pszOutFileName[XBAT_RES_FILE_NAME_LENGTH - 1] = _T('\0');
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

// This is resource extractor with decompression function, 
#ifdef MODE_FULL
BYTE* XBat_ExtractResourceEx(const LPVOID pData, DWORD dwSize, const BYTE* pKey, int keyLength, DWORD* pOutLen, TCHAR* pszOutFileName, DWORD* pdwOutAttrib) {
	if (!pData || !pKey) return NULL;
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)pData;
	if (pHeader->Magic != *(UINT*)XBatMagic) return NULL;
	
	if (pszOutFileName){
		_tcsncpy(pszOutFileName, pHeader->szFileName, XBAT_RES_FILE_NAME_LENGTH - 1);
		pszOutFileName[XBAT_RES_FILE_NAME_LENGTH - 1] = _T('\0');
	}
	if (pdwOutAttrib) *pdwOutAttrib = pHeader->dwAttributes;
	
	UINT SavedCrc = pHeader->SavedCrc;
	DWORD dwFinalSize = pHeader->dwOriginalSize;
	//DWORD dwCompressedSize = dwSize - (sizeof(XBAT_RES_HEADER) - 1); 
	// Safer version
	DWORD dwHeaderSize = (DWORD)((BYTE*)pHeader->Data - (BYTE*)pHeader);
	DWORD dwCompressedSize = dwSize - dwHeaderSize;
	
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
	_sntprintf(g_szSessionResPath, MAX_PATH, _T("%s\\Res"),g_szSessionDropPath);
	CreateDirectory(g_szSessionResPath, NULL);
	
}

//BOOL CALLBACK DbgEnumResNamesFunc(HMODULE hMod, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam){
//	MessageBox(NULL, _T("Inside Callback"), _T("Debug"), MB_OK);
//	return TRUE;
//}

// Use static variables to avoid stack problem in callback function
static TCHAR s_szFileName[MAX_PATH];
static TCHAR s_szFullPath[MAX_PATH];

BOOL CALLBACK EnumResNamesFunc(HMODULE hMod, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam){
	if (IS_INTRESOURCE(lpName)){
		HRSRC hRes = FindResource(hMod, lpName, RT_RCDATA);
		if (!hRes) return TRUE;
		
		DWORD dwSize = SizeofResource(hMod, hRes);
		HGLOBAL hData = LoadResource(hMod, hRes);
		void* pData = LockResource(hData);
		
		DWORD dwContentOutLen = 0;
		DWORD dwFileAttrib = 0;
		BYTE* pContent = NULL;
		
		// Clean old data
		s_szFileName[0] = _T('\0');
		
#ifdef MODE_FULL
		if (g_Config.GlobalFlags & XBAT_FLAG_LZMA_COMPRESSED){
			pContent = XBat_ExtractResourceEx(pData, dwSize, FinalKey, XBAT_FINAL_KEY_LENGTH, &dwContentOutLen, s_szFileName, &dwFileAttrib);
		} else {
			pContent = XBat_ExtractResource(pData, dwSize, FinalKey, XBAT_FINAL_KEY_LENGTH, &dwContentOutLen, s_szFileName, &dwFileAttrib);
		}
#else
		pContent = XBat_ExtractResource(pData, dwSize, FinalKey, XBAT_FINAL_KEY_LENGTH, &dwContentOutLen, s_szFileName, &dwFileAttrib);
#endif
		
		if (pContent && s_szFileName[0] != _T('\0')){
			// Safe string printer
			_sntprintf(s_szFullPath, MAX_PATH, _T("%s\\%s"), g_szSessionResPath, s_szFileName);
			s_szFullPath[MAX_PATH - 1] = _T('\0');
			
			HANDLE hFile = CreateFile(s_szFullPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				DWORD dwWritten;
				WriteFile(hFile, pContent, dwContentOutLen, &dwWritten, NULL);
				CloseHandle(hFile);
				SetFileAttributes(s_szFullPath, dwFileAttrib);
			}
			free(pContent);
		}
	}
	return TRUE;
}


BOOL DropScriptToTemp(BYTE* pBatContent, DWORD dwContentLen, TCHAR* szOutPath) {
	if (pBatContent == NULL || dwContentLen == 0 || szOutPath == NULL) return FALSE;
	
	if (!CreateDirectory(g_szSessionDropPath, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
	}
	
	TCHAR szRandom[5] = { 0 };
	XBat_GenerateRandomString(szRandom, 5);
	_sntprintf(szOutPath, MAX_PATH, _T("%s\\%s.bat"), g_szSessionDropPath, szRandom);
	
	HANDLE hFile = CreateFile(szOutPath, GENERIC_WRITE, 0, NULL, 
							  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	
	DWORD dwWritten = 0;
	BOOL bRet = WriteFile(hFile, pBatContent, dwContentLen, &dwWritten, NULL);
	CloseHandle(hFile);
	
	return bRet && (dwWritten == dwContentLen);
}

void RunBatAsFile_Legacy_Internal(TCHAR* lpszFilePath, BOOL bShow) {
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = bShow ? SW_SHOW : SW_HIDE;
	
	TCHAR szCmdLine[MAX_PATH + 32];
	_sntprintf(szCmdLine, _countof(szCmdLine), _T("cmd.exe /c \"%s\""), lpszFilePath);
	
	DWORD dwFlags = bShow ? 0 : CREATE_NO_WINDOW;
	if (CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, dwFlags, NULL, NULL, &si, &pi)) {
		// Wait for script to terminate
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

void ExecBat(BYTE* pBatContent, DWORD dwContentLen){
//	TCHAR szDbg[128];
//	wsprintf(szDbg, _T("Flags: 0x%08X, Pipe: %d, Full: %d"), 
//			 g_Config.GlobalFlags, 
//			 (g_Config.GlobalFlags & XBAT_FLAG_USE_PIPE) ? 1 : 0,
//			 #ifdef MODE_FULL
//			 1
//			 #else
//			 0
//			 #endif
//			 );
//	MessageBox(NULL, szDbg, _T("Trace"), MB_OK);
	
	BOOL bShowConsole = (g_Config.GlobalFlags & XBAT_FLAG_SHOW_CONSOLE);
	BOOL bUsePipe = FALSE;
	BOOL bRunAsFile = TRUE; // Lite stub run bat as file by default
	
#ifdef MODE_FULL
	bUsePipe = (g_Config.GlobalFlags & XBAT_FLAG_USE_PIPE);
	bRunAsFile = (g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE);	
#endif
	
	TCHAR szFilePath[MAX_PATH] = {0};
	
	// Init console
	if (bShowConsole && bUsePipe) {
#ifdef MODE_FULL
		SetupConsole(g_Config.szConsoleTitle);
#endif
	}
	
#ifdef MODE_FULL
	if (bUsePipe) {
		if (bRunAsFile) {
			// Fatih mode
			if (!DropScriptToTemp(pBatContent, dwContentLen, szFilePath)) {
				ShowErrorMessage(_T("DropScript(Pipe) Failed"));
				return;
			}
#ifdef UNICODE
			char* pszPathA = WCharToAnsi(szFilePath);
			RunBatPipe(NULL, 0, bShowConsole, pszPathA);
			if (pszPathA) free(pszPathA);
#else
			RunBatPipe(NULL, 0, bShowConsole, szFilePath);
#endif
		} else {
			// Full pipe mode
			RunBatPipe((LPCSTR)pBatContent, dwContentLen, bShowConsole, NULL);
		}
		return; // Exite after pipe mode
	}
#endif
	
	// Legacy mode
	// Fallback for lite mode and pipe not closed
	if (DropScriptToTemp(pBatContent, dwContentLen, szFilePath)) {
		RunBatAsFile_Legacy_Internal(szFilePath, bShowConsole);
	} else {
		ShowErrorMessage(_T("DropScript(Legacy) Failed"));
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
			_sntprintf(szInjectHeader, _countof(szInjectHeader), _T("@set \"RESDIR=%s\"\n@set \"EXEPATH=%s\"\n"), g_szSessionResPath, szExePath);
			
#ifdef UNICODE
			// Convert wide header in UNICODE to ANSI for script
			int nHeaderLen = WideCharToMultiByte(CP_ACP, 0, szInjectHeader, -1, NULL, 0, NULL, NULL) - 1;
			char* pHeaderA = (char*)malloc(nHeaderLen + 1);
			WideCharToMultiByte(CP_ACP, 0, szInjectHeader, -1, pHeaderA, nHeaderLen + 1, NULL, NULL);
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
			
//			// g_szSessionDropPath backup
//			TCHAR szSessionDropPath[MAX_PATH];
//			memcpy(szSessionDropPath, g_szSessionDropPath, sizeof(szSessionDropPath));
			
			EnumResourceNames(hMod, RT_RCDATA, EnumResNamesFunc, 0);
			
//			// Recover g_szSessionDropPath
//			memcpy(g_szSessionDropPath, szSessionDropPath, sizeof(g_szSessionDropPath));
		} 
		
		//MessageBox(NULL, _T("About to enter ExecBat"), _T("msg"), MB_OK);
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
		
		_tcscpy(szDelPath, g_szSessionDropPath);
		
		shfo.pFrom = szDelPath;
		shfo.wFunc = FO_DELETE;
		shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;
		
		SHFileOperation(&shfo);
	}
	
	
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
	_sntprintf(g_szSessionDropPath, MAX_PATH, _T("%s\\XBAT.%s"), szTempPath, szRandom);
	
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
	
	return 0;
}

