#include "stub_full.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include "../../common/lzma_sdk/lzmadec/LzmaDec.h"

#include <process.h>

#include <conio.h>
#include <io.h>
#include <fcntl.h>

#include "../../common/vc6_patch.h"

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

unsigned CALLBACK OutputReaderThread(LPVOID lpParam){
	THREAD_PARAMS* pParams = (THREAD_PARAMS*)lpParam;
	char buffer[4096];
	DWORD dwRead;

	while(ReadFile(pParams->hPipeRead, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0){
		if (!pParams->bSilent){
			buffer[dwRead] = '\0';
			printf("%s", buffer);
		}
	}
	free(pParams);
	return 0;
}

BOOL RunBatPipe(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR pBatPath) {
	HANDLE hInRead, hInWrite, hOutRead, hOutWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	
	if (pBatContent && dwSize == 0) return FALSE;
	
	if (!pBatPath && pBatContent){
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
	if (pBatPath) {
		// Fatih-like mode
		_snprintf(cmdLine, SAFE_LEN(sizeof(cmdLine)), "cmd.exe /Q /D /C \"\"%s\"\"", pBatPath);
		SET_STOPPER(cmdLine, sizeof(cmdLine));
	} else {
		// Memory mode
		_snprintf(cmdLine, SAFE_LEN(sizeof(cmdLine)), "cmd.exe /Q /D /K \"@echo off\"");
		SET_STOPPER(cmdLine, sizeof(cmdLine));
	}
	
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = bShowConsole ? SW_SHOW : SW_HIDE;
	if (pBatPath) {
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
	
	// Solve the appstarting cursor problem
	MSG msg;
	PostThreadMessage(GetCurrentThreadId(), WM_USER, 0, 0);
	PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
	
	if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		// Clean
		if (!pBatPath) {
			CloseHandle(hInRead); CloseHandle(hInWrite);
			CloseHandle(hOutRead); CloseHandle(hOutWrite);
		}
		return FALSE;
	}
	
	
	if (!pBatPath) {
		CloseHandle(hInRead);
		CloseHandle(hOutWrite);
		
		THREAD_PARAMS* pParams = (THREAD_PARAMS*)malloc(sizeof(THREAD_PARAMS));
		if (pParams) {
			pParams->hPipeRead = hOutRead;
			pParams->bSilent = !bShowConsole;
			_beginthreadex(NULL, 0, OutputReaderThread, pParams, 0, NULL);
		}
		
		DWORD dwWritten;
		WriteFile(hInWrite, "\n", 1, &dwWritten, NULL);
		WriteFile(hInWrite, pBatContent, dwSize, &dwWritten, NULL);
		WriteFile(hInWrite, "\nexit\n", 6, &dwWritten, NULL);
		
		CloseHandle(hInWrite);
	}
	
	// Wait
	if (bShowConsole) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	
	if (!pBatPath) {
		CloseHandle(hOutRead);
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	return TRUE;
}


void SetupConsole(LPCSTR lpszConsoleTitle){
	// Allocate console
	if (!AllocConsole()) return; // Return if console already exists
	
	FILE* fpOut = NULL;
	freopen_s(&fpOut, "CONOUT$", "w", stdout);
	
	FILE* fpIn = NULL;
	freopen_s(&fpIn, "CONIN$", "r", stdin);
	
	FILE* fpErr = NULL;
	freopen_s(&fpErr, "CONOUT$", "w", stderr);
	
	SetConsoleTitleA(lpszConsoleTitle);
	
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stdin, NULL, _IONBF, 0);
	
}



