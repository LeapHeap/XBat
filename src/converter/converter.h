#pragma once

#include <windows.h>
#include <vector>
#include <string>

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
	BOOL bUseUpx;
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

extern TCHAR g_szTempWorkDirPath[MAX_PATH];
extern TCHAR g_szTempArchivePath[MAX_PATH];
extern TCHAR g_szConverterExePath[MAX_PATH];
extern TCHAR g_szConverterDirPath[MAX_PATH];


BOOL InitTempWorkDir();
int DestroyTempWorkDir();
LPBYTE LoadFileToBuffer(LPCTSTR lpszFilePath, LPDWORD lpdwSize);
BOOL PackAndInjectResourceEx(
	HANDLE hUpdate,
	int ResourceID,
	const LPBYTE lpRawData,
	DWORD dwDataLen,
	const LPBYTE lpKey,
	LPCTSTR lpszTargetFileName,
	DWORD dwAttrib
);
void ConverterProcess(CONVERTER_LIST* lpList);






