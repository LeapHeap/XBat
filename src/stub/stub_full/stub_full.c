#include "../../common/nocrt_patch.h"

#include "stub_full.h"
#include <windows.h>
#include "../../common/lzma_sdk/lzmadec/LzmaDec.h"

// LZMA sdk allocator
static void* SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void* address) { free(address); }
static ISzAlloc g_LzmaAlloc = { SzAlloc, SzFree };

BYTE* XBat_DecompressBuffer(const BYTE* pCompressedData, DWORD dwCompressedSize, DWORD dwExpectedSize) {
	if (!pCompressedData || dwCompressedSize <= LZMA_PROPS_SIZE) {
		return NULL;
	}
	
	BYTE* pDecompressedBuf = (BYTE*)malloc(dwExpectedSize);
	if (!pDecompressedBuf) return NULL;
	
	SizeT destLen = (SizeT)dwExpectedSize;
	SizeT srcLen = (SizeT)(dwCompressedSize - LZMA_PROPS_SIZE);
	ELzmaStatus status;

	SRes res = LzmaDecode(
						  pDecompressedBuf, &destLen,
						  pCompressedData + LZMA_PROPS_SIZE, &srcLen,
						  pCompressedData, LZMA_PROPS_SIZE,
						  LZMA_FINISH_ANY, &status, &g_LzmaAlloc
						  );
	
	if (res != SZ_OK || destLen != (SizeT)dwExpectedSize) {
		free(pDecompressedBuf);
		return NULL;
	}
	
	return pDecompressedBuf;
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

