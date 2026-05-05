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

#ifdef FUCKED
BOOL ExecuteMemoryScript(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole){
	HANDLE hInRead, hInWrite; // Stdin pipe
	HANDLE hOutRead, hOutWrite; // Stdout pipe
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE }; // Allow handle inheritance
	
	CreatePipe(&hInRead, &hInWrite, &sa, 0);
	CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
	SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
	
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = hInRead; // Redirect input
	si.hStdOutput = hOutWrite;
	si.hStdError = hOutWrite;
	
	char cmdLine[] = "cmd.exe /Q /K";
	if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return FALSE;
	
	// Close hanldes inherited by child process
	CloseHandle(hInRead);
	CloseHandle(hOutWrite);
	
	// Start output reader thread
	THREAD_PARAMS* pParams = (THREAD_PARAMS*)malloc(sizeof(THREAD_PARAMS));
	pParams->hPipeRead = hOutRead; // Pass the output reading handle
	_beginthreadex(NULL, 0, OutputReaderThread, pParams, 0, NULL);
	
	DWORD dwWritten;
	WriteFile(hInWrite, pBatContent, dwSize, &dwWritten, NULL);
	
	if (bShowConsole){
		while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
			if (_kbhit()) {
				char c = _getch();
				if (c == 13) c = '\n';
				WriteFile(hInWrite, &c, 1, &dwWritten, NULL);
			}
		}
	} else {
		// Silent
		// Send exit command to terminate cmd
		const CHAR* pExit = "\nexit\n";
		WriteFile(hInWrite, pExit, (DWORD)strlen(pExit), &dwWritten, NULL);
		// Wait for cmd to end
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	
	// Clean handles
	CloseHandle(hInWrite);
	CloseHandle(hOutRead);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	return TRUE;
}
#endif

// If you wanna use this, you must use standard .cmd or .bat suffix for temp script file.
#define USE_BAT_SUFFIX

#ifdef NIGGER
/**
 * 管道执行核心函数 (RunBatPipe)
 * @param pBatContent 脚本内容 (内存模式使用，文件模式传NULL)
 * @param dwSize      内容长度 (内存模式专用，文件模式传0）
 * @param bShowConsole 是否回显到控制台
 * @param pBatPath    落地的脚本路径 (文件模式使用，内存模式传NULL)
 */
BOOL RunBatPipe(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR pBatPath) {
	HANDLE hInRead, hInWrite;
	HANDLE hOutRead, hOutWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	
	if (pBatContent){
		if (dwSize == 0) return FALSE;
	}
	
	// 1. 创建管道
	if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return FALSE;
	if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) { // 修正了原代码的 bug
		CloseHandle(hInRead); CloseHandle(hInWrite);
		return FALSE;
	}
	
	SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
	
	// 2. 构造命令行
	char cmdLine[MAX_PATH + 32];
	if (pBatPath) {
		// 半管道文件模式：使用 /C 执行完即退出
		sprintf_s(cmdLine, sizeof(cmdLine), "cmd.exe /Q /C \"%s\"", pBatPath);
	} else {
		// 全管道内存模式：使用 /K 保持运行以便灌入流
		strcpy_s(cmdLine, sizeof(cmdLine), "cmd.exe /Q /K");
	}
	
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE; // 傀儡进程始终隐藏
	si.hStdInput = hInRead;
	si.hStdOutput = hOutWrite;
	si.hStdError = hOutWrite;
	
	// 3. 启动进程
	if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
		// 清理并退出
		return FALSE;
	}
	
	CloseHandle(hInRead);
	CloseHandle(hOutWrite);
	
	// 4. 启动输出读取线程 (不管显示不显示，都要抽水防止死锁)
	THREAD_PARAMS* pParams = (THREAD_PARAMS*)malloc(sizeof(THREAD_PARAMS));
	pParams->hPipeRead = hOutRead;
	pParams->bSilent = !bShowConsole; // 之前助手中提到的静默标志
	_beginthreadex(NULL, 0, OutputReaderThread, pParams, 0, NULL);
	
	DWORD dwWritten;
	// 5. 如果是内存模式，灌入内容
	if (!pBatPath && pBatContent) {
		WriteFile(hInWrite, pBatContent, dwSize, &dwWritten, NULL);
	}
	
	// 6. 交互处理
	if (bShowConsole) {
		// 在显示控制台的情况下，始终进入键盘转发循环
		// 如果是文件模式，CMD 运行完脚本会自行退出，循环随之结束
		while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
			if (_kbhit()) {
				char c = _getch();
				if (c == 13) c = '\n';
				WriteFile(hInWrite, &c, 1, &dwWritten, NULL);
			}
		}
	} else {
		// 静默模式
		if (!pBatPath) {
			// 内存模式需要额外发 exit
			const CHAR* pExit = "\nexit\n";
			WriteFile(hInWrite, pExit, (DWORD)strlen(pExit), &dwWritten, NULL);
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	
	// 7. 清理
	CloseHandle(hInWrite);
	CloseHandle(hOutRead);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	return TRUE;
}
#endif

BOOL RunBatPipe(LPCSTR pBatContent, DWORD dwSize, BOOL bShowConsole, LPCSTR pBatPath) {
	HANDLE hInRead, hInWrite, hOutRead, hOutWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
	
	if (pBatContent && dwSize == 0) return FALSE;
	
	// 1. 创建管道
	if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return FALSE;
	if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
		CloseHandle(hInRead); CloseHandle(hInWrite);
		return FALSE;
	}
	
	SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
	
	// 2. 构造命令行
	char cmdLine[MAX_PATH + 32];
	if (pBatPath) {
		sprintf_s(cmdLine, sizeof(cmdLine), "cmd.exe /Q /C \"%s\"", pBatPath);
	} else {
		strcpy_s(cmdLine, sizeof(cmdLine), "cmd.exe /Q /K");
		//sprintf_s(cmdLine, sizeof(cmdLine), "cmd.exe /Q /D /K \"@chcp 65001 >nul\"");
	}
	
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = hInRead;
	si.hStdOutput = hOutWrite;
	si.hStdError = hOutWrite;
	
	// 3. 启动进程 (增加 CREATE_NEW_PROCESS_GROUP 提高稳定性)
	if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
		return FALSE;
	}
	
	CloseHandle(hInRead);
	CloseHandle(hOutWrite);
	
	// 4. 启动输出读取线程
	THREAD_PARAMS* pParams = (THREAD_PARAMS*)malloc(sizeof(THREAD_PARAMS));
	if (pParams) {
		pParams->hPipeRead = hOutRead;
		pParams->bSilent = !bShowConsole;
		_beginthreadex(NULL, 0, OutputReaderThread, pParams, 0, NULL);
	}
	
	DWORD dwWritten;
	
	// 5. 内存灌入模式 (非文件模式时执行)
	if (!pBatPath && pBatContent) {
		
		// 一次性写入全部内容
		WriteFile(hInWrite, pBatContent, dwSize, &dwWritten, NULL);
		
		// 强制补两个换行，防止最后一行因为没有回车不执行
		WriteFile(hInWrite, "\n\n", 2, &dwWritten, NULL);
		
		
		// 如果是静默模式，直接灌入 exit 结束进程
		if (!bShowConsole) {
			const CHAR* pExit = "\nexit\n";
			WriteFile(hInWrite, pExit, (DWORD)strlen(pExit), &dwWritten, NULL);
		}
	}
	
	// 6. 交互与退出处理
	if (bShowConsole) {
		while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT) {
			if (_kbhit()) {
				char c = _getch();
				if (c == 13) c = '\n';
				WriteFile(hInWrite, &c, 1, &dwWritten, NULL);
			}
		}
	} else {
		// 静默模式
		if (!pBatPath) {
			const CHAR* pExit = "\nexit\n";
			WriteFile(hInWrite, pExit, (DWORD)strlen(pExit), &dwWritten, NULL);
		}
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	
	// 7. 清理
	CloseHandle(hInWrite);
	CloseHandle(hOutRead);
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



