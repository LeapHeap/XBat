
#ifndef BUILDING
// For editor preview
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#endif

#include <windows.h>
// Memory function shim
#ifdef __cplusplus
extern "C" {
#endif
	
	
#undef memcpy
#undef memset
#undef RtlMoveMemory
#undef RtlFillMemory
	
	
	__declspec(dllimport)
	void __stdcall RtlMoveMemory(
								 void* Destination,
								 const void* Source,
								 size_t Length
								 );
	
	__declspec(dllimport)
	void __stdcall RtlFillMemory(
								 void* Destination,
								 size_t Length,
								 unsigned char Fill
								 );
	
	void* memcpy(void* dest, const void* src, size_t count)
	{
		RtlMoveMemory(dest, src, count);
		return dest;
	}
	
	void* memset(void* dest, int ch, size_t count)
	{
		RtlFillMemory(dest, count, (unsigned char)ch);
		return dest;
	}
	
	
#ifdef __cplusplus
}
#endif

#include "../common/nocrt_patch.h"

#include "../common/shared_defs.h"
#include "../common/crypto.h"
#include "../common/Utils.h"

HMODULE g_hStub;
BYTE FinalKey[XBAT_FINAL_KEY_LENGTH];
BYTE XBatMagic[] = XBAT_MAGIC_DATA;
XBAT_CONFIG g_Config;
TCHAR g_szSessionDropPath[MAX_PATH];
TCHAR g_szSessionResPath[MAX_PATH];
TCHAR g_szSessionScriptPath[MAX_PATH];
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
	wnsprintf(szBuf, _countof(szBuf), _T("%s. Error: %u (0x%08X)"), lpszMsg, dwErr, dwErr);
	
	MessageBox(NULL, szBuf, _T("Stub Error"), MB_ICONERROR);
}

BOOL DirectoryExists(LPCTSTR lpszPath)
{
	DWORD dwAttrib = GetFileAttributes(lpszPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SetDefaultConfig(XBAT_CONFIG* pCfg){
	pCfg->Magic = *(UINT*)XBatMagic;
	pCfg->GlobalFlags = 0;
	pCfg->DropDirType = XBAT_DROP_DIR_TEMP;
	lstrcpyA(pCfg->szConsoleTitle, "XBat Executor Console");
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
LPBYTE XBat_ExtractResource(const LPVOID lpData, DWORD dwSize, const LPBYTE lpKey, int keyLength, LPDWORD lpOutLen, LPTSTR lpszOutFileName, LPDWORD lpdwOutAttrib){
	if (!lpData || !lpKey) return NULL;
	XBAT_RES_HEADER* pHeader = (XBAT_RES_HEADER*)lpData;
	UINT Magic = pHeader->Magic;
	if (Magic != *(UINT*)XBatMagic) return NULL;
	
	if (lpszOutFileName){
		lstrcpyn(lpszOutFileName, pHeader->szFileName, XBAT_RES_FILE_NAME_LENGTH);
	}
	
	if (lpdwOutAttrib) {
		*lpdwOutAttrib = pHeader->dwAttributes;
	}
	
	
	UINT SavedCrc = pHeader->SavedCrc;
	DWORD dwRawSize = pHeader->dwOriginalSize;
	
	
	BYTE* pContent = (BYTE*)malloc(dwRawSize);
	if (!pContent) return NULL;
	
	memcpy(pContent,pHeader->Data,dwRawSize);
	RC4_CTX ctx;
	RC4_Init(&ctx,lpKey,keyLength);
	RC4_Process(&ctx,pContent,dwRawSize);
	
	if (CalculateCRC32(pContent,dwRawSize) != SavedCrc){
		free(pContent);
		return NULL;
		
	}
	
	if (lpOutLen) *lpOutLen = dwRawSize;
	return pContent;
	
}


void InitResPath(){
	wnsprintf(g_szSessionResPath, MAX_PATH, _T("%s\\Res"),g_szSessionDropPath);
	CreateDirectory(g_szSessionResPath, NULL);
}

//BOOL CALLBACK DbgEnumResNamesFunc(HMODULE hMod, LPCTSTR lpType, LPTSTR lpName, LONG_PTR lParam){
//	MessageBox(NULL, _T("Inside Callback"), _T("Debug"), MB_OK);
//	return TRUE;
//}


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
		
		TCHAR* pLocalFileName = (TCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_PATH * sizeof(TCHAR));
		TCHAR* pLocalFullPath = (TCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_PATH * sizeof(TCHAR));
		
		if (!pLocalFileName || !pLocalFullPath) {
			if (pLocalFileName) HeapFree(GetProcessHeap(), 0, pLocalFileName);
			if (pLocalFullPath) HeapFree(GetProcessHeap(), 0, pLocalFullPath);
			return TRUE;
		}
		
		// Clean old data
		pLocalFileName[0] = _T('\0');
		
		pContent = XBat_ExtractResource(pData, dwSize, FinalKey, XBAT_FINAL_KEY_LENGTH, &dwContentOutLen, pLocalFileName, &dwFileAttrib);
		
		if (pContent && pLocalFileName[0] != _T('\0')){
			// Safe string printer
			wnsprintf(pLocalFullPath, MAX_PATH, _T("%s\\%s"), g_szSessionResPath, pLocalFileName);
			
			HANDLE hFile = CreateFile(pLocalFullPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE) {
				DWORD dwWritten;
				WriteFile(hFile, pContent, dwContentOutLen, &dwWritten, NULL);
				CloseHandle(hFile);
				SetFileAttributes(pLocalFullPath, dwFileAttrib);
			}
			free(pContent);
		}
		
		HeapFree(GetProcessHeap(), 0, pLocalFileName);
		HeapFree(GetProcessHeap(), 0, pLocalFullPath);
		
	}
	return TRUE;
}


BOOL DropScriptToTemp(LPBYTE lpBatContent, DWORD dwContentLen, LPTSTR lpszOutPath) {
	
	if (lpBatContent == NULL || dwContentLen == 0 || lpszOutPath == NULL) return FALSE;
	
	if (!CreateDirectory(g_szSessionDropPath, NULL)) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) return FALSE;
	}
	
	TCHAR szRandom[5] = { 0 };
	XBat_GenerateRandomString(szRandom, 5);
	wnsprintf(lpszOutPath, MAX_PATH, _T("%s\\%s.bat"), g_szSessionDropPath, szRandom);
	
	HANDLE hFile = CreateFile(lpszOutPath, GENERIC_WRITE, 0, NULL, 
							  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	
	lstrcpyn(g_szSessionScriptPath, lpszOutPath, MAX_PATH);
	
	DWORD dwWritten = 0;
	BOOL bRet = WriteFile(hFile, lpBatContent, dwContentLen, &dwWritten, NULL);
	CloseHandle(hFile);
	
	return bRet && (dwWritten == dwContentLen);
}

static void RunBatAsFile_Legacy_Internal(LPTSTR lpszFilePath, BOOL bShow) {
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = bShow ? SW_SHOW : SW_HIDE;
	
	TCHAR szCmdLine[MAX_PATH + 32];
	wnsprintf(szCmdLine, _countof(szCmdLine), _T("cmd.exe /c \"%s\""), lpszFilePath);
	
	DWORD dwFlags = bShow ? 0 : CREATE_NO_WINDOW;
	if (CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, dwFlags, NULL, NULL, &si, &pi)) {
		// Wait for script to terminate
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}

typedef struct{
	HANDLE hPipeRead;
	BOOL bSilent;
} THREAD_PARAMS;

DWORD WINAPI OutputReaderThread(LPVOID lpParam)
{
	THREAD_PARAMS* pParams = (THREAD_PARAMS*)lpParam;
	if (pParams == NULL) return 0;
	
	char buffer[4096];
	DWORD dwRead;
	DWORD dwWritten;
	
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	
	while (ReadFile(pParams->hPipeRead, buffer, sizeof(buffer), &dwRead, NULL) && dwRead > 0)
	{
		if (!pParams->bSilent && hStdOut != NULL && hStdOut != INVALID_HANDLE_VALUE)
		{
			WriteFile(hStdOut, buffer, dwRead, &dwWritten, NULL);
		}
	}
	
	CloseHandle(pParams->hPipeRead);
	
	free(pParams);
	
	return 0;
}

BOOL RunBatPipe(LPCSTR lpBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR lpszBatPath) {
	HANDLE hInRead, hInWrite, hOutRead, hOutWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	
	if (lpBatContent && dwSize == 0) return FALSE;
	
	if (!lpszBatPath && lpBatContent){
		if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return FALSE;
		if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
			CloseHandle(hInRead); CloseHandle(hInWrite);
			return FALSE;
		}
		SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
	}
	
	char cmdLine[MAX_PATH + 128];
	ZeroMemory(cmdLine, sizeof(cmdLine));
	if (lpszBatPath) {
		// Fatih-like mode
		wnsprintfA(cmdLine, sizeof(cmdLine), "cmd.exe /Q /D /C \"\"%s\"\"", lpszBatPath);
	} else {
		// Memory mode
		wnsprintfA(cmdLine, sizeof(cmdLine), "cmd.exe /Q /D /K \"@echo off\"");
	}
	
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = bShowConsole ? SW_SHOW : SW_HIDE;
	if (lpszBatPath) {
		// Fatih mode: inherit stdin handle of parent process
		si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
	} else {
		// Memory mode: redirect to pipe
		si.hStdInput = hInRead;
		si.hStdOutput = hOutWrite;
		si.hStdError = hOutWrite;
	}
	
	// Solve the appstarting cursor issue
	MSG msg;
	PostThreadMessage(GetCurrentThreadId(), WM_USER, 0, 0);
	PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
	
	if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		// Clean
		if (!lpszBatPath) {
			CloseHandle(hInRead); CloseHandle(hInWrite);
			CloseHandle(hOutRead); CloseHandle(hOutWrite);
		}
		return FALSE;
	}
	
	
	if (!lpszBatPath && lpBatContent) {
		CloseHandle(hInRead);
		CloseHandle(hOutWrite);
		
		THREAD_PARAMS* pParams = (THREAD_PARAMS*)malloc(sizeof(THREAD_PARAMS));
		if (pParams) {
			pParams->hPipeRead = hOutRead;
			pParams->bSilent = !bShowConsole;
			
			HANDLE hThread = CreateThread(NULL, 0, OutputReaderThread, pParams, 0, NULL);
			if (hThread != NULL) {
				CloseHandle(hThread);
			} else {
				free(pParams);
			}
			
		}
		
		DWORD dwWritten;
		WriteFile(hInWrite, "\n", 1, &dwWritten, NULL);		
		WriteFile(hInWrite, lpBatContent, dwSize, &dwWritten, NULL);
		WriteFile(hInWrite, "\nexit\n", 6, &dwWritten, NULL);
		
		CloseHandle(hInWrite);
	}
	
	// Wait
	if (bShowConsole) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	return TRUE;
}

void ExecBat(LPBYTE lpBatContent, DWORD dwContentLen){
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
	
	bUsePipe = (g_Config.GlobalFlags & XBAT_FLAG_USE_PIPE);
	bRunAsFile = (g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE);	
	
	static TCHAR szFilePath[MAX_PATH];
	memset(szFilePath, 0, sizeof(szFilePath));
	
	// Init console
	if (bShowConsole && bUsePipe) {
#ifdef MODE_CLI
		SetConsoleTitleA(g_Config.szConsoleTitle);
#endif
	}
	
	if (bUsePipe) {
		if (bRunAsFile) {
			// Fatih mode
			if (!DropScriptToTemp(lpBatContent, dwContentLen, szFilePath)) {
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
			RunBatPipe((LPCSTR)lpBatContent, dwContentLen, bShowConsole, NULL);
		}
		return; // Exite after pipe mode
	}
	
	// Legacy mode
	// Fallback for lite mode and pipe not closed
	if (DropScriptToTemp(lpBatContent, dwContentLen, szFilePath)) {
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
	

	pBatContent = XBat_ExtractResource(pBatData,dwBatResSize,FinalKey,XBAT_FINAL_KEY_LENGTH,&dwBatContentOutLen,NULL,NULL);
	
	
	if (pBatContent){
		BYTE* pFinalContent = pBatContent;
		DWORD dwFinalLen = dwBatContentOutLen;
		
		// If has user resources
		if (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES){
			
			char szExePathA[MAX_PATH];
			GetModuleFileNameA(NULL, szExePathA, MAX_PATH);
			
			char* pLastSlashA = strrchr(szExePathA, '\\');
			if (pLastSlashA) *pLastSlashA = '\0';
			
			char szResPathA[MAX_PATH];
			WideCharToMultiByte(CP_ACP, 0, g_szSessionResPath, -1, szResPathA, MAX_PATH, NULL, NULL);
			
			char szInjectHeaderA[MAX_PATH * 3];
			wnsprintfA(szInjectHeaderA, _countof(szInjectHeaderA), "@set \"RESDIR=%s\"\r\n@set \"EXEDIR=%s\"\r\n", szResPathA, szExePathA);
			
			size_t nHeaderLen = lstrlenA(szInjectHeaderA); 
			size_t totalLen = nHeaderLen + (size_t)dwBatContentOutLen;
			
			pFinalContent = (BYTE*)malloc(totalLen);
			if (pFinalContent) {
				memcpy(pFinalContent, szInjectHeaderA, nHeaderLen);
				memcpy(pFinalContent + nHeaderLen, pBatContent, dwBatContentOutLen);
				dwFinalLen = (DWORD)totalLen;
			}
			
			free(pBatContent); 
			
			EnumResourceNames(hMod, RT_RCDATA, EnumResNamesFunc, 0);
		} 
		
		// Call 7zdec to decompress dir_pack.7z and clean if needed
		if (g_Config.GlobalFlags & XBAT_FLAG_CALL_7ZDEC){
			TCHAR szDecoderPath[MAX_PATH];
			wnsprintf(szDecoderPath, MAX_PATH, _T("%s\\%s"), g_szSessionResPath, k_lpszArchiveDecoderName);
			TCHAR szArchivePath[MAX_PATH];
			wnsprintf(szArchivePath, MAX_PATH, _T("%s\\%s"), g_szSessionResPath, k_lpszArchiveFileName);
			
			STARTUPINFO si;
			ZeroMemory(&si, sizeof(si));
			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));
			si.cb = sizeof(si);
			static TCHAR szCmdLine[(MAX_PATH*3) + 128];
			wnsprintf(szCmdLine, _countof(szCmdLine), _T("\"%s\" x \"%s\""), szDecoderPath, szArchivePath);
			if (!CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g_szSessionResPath, &si, &pi)){
				ShowErrorMessage(_T("Failed to call archive decoder"));
			}
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			DeleteFile(szDecoderPath);
			DeleteFile(szArchivePath);
		}
		
		
		ExecBat(pFinalContent, dwFinalLen);
		
		if (pFinalContent) free(pFinalContent);
		
	} else {
		MessageBox(NULL,_T("Failed to load script"),_T("Error"),MB_ICONERROR);
	}
	
	
}


void StubDestroy() {
	if (!DirectoryExists(g_szSessionDropPath)) return;
	
	SHFILEOPSTRUCT shfo;
	TCHAR szDelPath[MAX_PATH + 2]; 
	ZeroMemory(szDelPath, sizeof(szDelPath));
	ZeroMemory(&shfo, sizeof(shfo));
	
	BOOL bDeleteRoot = TRUE;
	
	if (!(g_Config.GlobalFlags & XBAT_FLAG_DESTROY_RESOURCES) && 
		(g_Config.DropDirType != XBAT_DROP_DIR_TEMP)) 
	{
		bDeleteRoot = FALSE;
	}
	
	if (bDeleteRoot) {
		lstrcpyn(szDelPath, g_szSessionDropPath, MAX_PATH);
	} else {
		// Delete script only
		if (lstrlen(g_szSessionScriptPath) > 0) {
			lstrcpyn(szDelPath, g_szSessionScriptPath, MAX_PATH);
		} else {
			return;
		}
	}
	
	// SHFileOperation must be terminated with double NULL
	size_t nLen = lstrlen(szDelPath);
	szDelPath[nLen] = _T('\0');
	szDelPath[nLen + 1] = _T('\0');
	
	shfo.pFrom = szDelPath;
	shfo.wFunc = FO_DELETE;
	// FOF_NOERRORUI:
	shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
	
	SHFileOperation(&shfo);
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
	wnsprintf(g_szSessionDropPath, MAX_PATH, _T("%s\\XBAT.%s"), szTempPath, szRandom);
	
	// Create dir
	CreateDirectory(g_szSessionDropPath, NULL);
}

//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
//	g_hStub = GetModuleHandle(NULL);
//	
//	InitGlobalConfig(g_hStub);
//	
//	if ((g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE) || (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES)){
//		InitSessionDropPath();
//		if (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES) InitResPath();
//	}
//	
//	StubProcess();
//	
//	if (g_Config.GlobalFlags & XBAT_FLAG_SELF_DESTROY){
//		StubDestroy();
//	}
//	
//	return 0;
//}

#ifdef __cplusplus
extern "C" {
#endif
		
	void RealMain(void){
		g_hStub = GetModuleHandle(NULL);
		
		InitGlobalConfig(g_hStub);
		
		if ((g_Config.GlobalFlags & XBAT_FLAG_RUN_BAT_AS_FILE) || (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES)){
			InitSessionDropPath();
			if (g_Config.GlobalFlags & XBAT_FLAG_HAS_USER_RESOURCES) InitResPath();
		}
		
		StubProcess();
		
		if (g_Config.GlobalFlags & XBAT_FLAG_SELF_DESTROY){
			StubDestroy();
		}
		
		ExitProcess(0);
	}
	
	void WINAPI MyMain(void)
	{
		#if defined(_WIN64)
		__asm__ (
				 "andq $-16, %rsp\n\t"
				 "subq $32, %rsp\n\t"
				 "call RealMain\n\t"
				 );
#else
		RealMain(); 
#endif
	}
	
#ifdef __cplusplus
}
#endif


