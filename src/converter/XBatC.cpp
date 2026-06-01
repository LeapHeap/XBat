#include <Windows.h>
#include <stdio.h>
#include <tchar.h>
#include <vector>
#include <string>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#ifdef __cplusplus
extern "C" {
#endif
#include "../common/shared_defs.h"
#include "../common/version.h"
#ifdef __cplusplus
}
#endif

#include "converter.h"

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

static BOOL ParseCmdLine(int argc, char* argv[], CONVERTER_OPTIONS* lpOpt) {
	ZeroMemory(lpOpt, sizeof(CONVERTER_OPTIONS));

	// Default behavior
	lpOpt->bShowConsole = TRUE;
	lpOpt->eMode = MODE_FATIH;
	lpOpt->bDestroyRes = TRUE;
	lpOpt->bEnableLzma = FALSE;	// Deprecated
	lpOpt->bHasUserRes = FALSE;
	lpOpt->nDropDirType = XBAT_DROP_DIR_TEMP;
	lpOpt->bUseUpx = FALSE;

	if (argc < 2) {
		fprintf(stderr, "Error: Too few arguments. For command line usage, use /? or /help.\n");
		return FALSE;
	}

	for (int i = 1; i < argc; ++i) {
		const char* lpszArg = argv[i];

		if (CheckArgPattern("/?") || CheckArgPattern("/help")) {
			printf("======================================================================\n");
			printf(" XBat CLI v%s - Batch Script to Executable Compiler\n", FILE_VERSION_STRING);
			printf("======================================================================\n\n");
			printf("Usage:\n");
			printf("  %s /bat <BatScriptPath> [options...]\n\n", XBAT_CLI_EXE_NAME);

			printf("Required Arguments:\n");
			printf("  /bat <path>        Path to the source batch file (.bat / .cmd) to compile.\n\n");

			printf("Optional Output Settings:\n");
			printf("  /exe <path>        Path to the target output executable (.exe).\n");
			printf("                     (Default: generated in the same dir with the same name as source batch)\n");
			printf("  /x64               Build a native 64-bit executable.\n");
			printf("                     (Default: build 32-bit x86 executable)\n");
			printf("  /upx               Compress the final executable using UPX.\n");
			printf("                     (WARNING: UPX may cause problems, including resource fails. Be careful to use it.)\n\n");

			printf("Execution Modes:\n");
			printf("  /mode <type>       Set payload execution backend environment. Supported modes:\n");
			printf("                       standard : Extracts assets to dropdir and runs securely. (Default)\n");
			printf("                       memory   : Pure in-memory streaming execution via pipes.\n");
			printf("                       (WARNING: memory mode is experimental and does NOT support\n");
			printf("                       interactive commands, e.g. pause or set /p.)\n");
			printf("                       lite     : Simple script execution without creating a pipe.\n");
			printf("  /invisible         Hide the application console window on payload startup.\n");
			printf("                     (Default: console window is visible)\n\n");

			printf("Resource & Cleanup Options:\n");
			printf("  /include <path>    Include external user resource files or directories.\n");
			printf("                     (Can be specified multiple times to package multiple folders)\n");
			printf("  /icon <path>       Inject custom .ico application icon into the target EXE.\n");
			printf("  /ver <path>        Inject custom PE version information via an .ini file template.\n");
			printf("  /dropdir <int>     Set the drop extraction target location for standard mode:\n");
			printf("                       0 : User's local %%TEMP%% directory. (Default)\n");
			printf("                       1 : Current execution directory.\n");
			printf("  /noclean           Disable post-execution asset cleanup.\n");
			printf("                     (Default: completely purges session data on exit)\n\n");

			printf("======================================================================\n");
			printf(" IMPORTANT: SCRIPT DEVELOPMENT ENVIRONMENT GUIDE\n");
			printf("======================================================================\n");
			printf("  1. SCRIPT ENCODING WARNING (CRITICAL):\n");
			printf("     The source batch file MUST be saved in native ANSI encoding (e.g., GBK for\n");
			printf("     Chinese Windows, Code Page 936). Do NOT use UTF-8! UTF-8 scripts containing\n");
			printf("     non-ASCII characters will cause messy code and runtime failures.\n\n");

			printf("  2. WORKPATH & PATH LOCATOR:\n");
			printf("     DO NOT use '%%~dp0' in your batch script to locate files! Due to the isolated\n");
			printf("     and in-memory extraction backend, '%%~dp0' will point to incorrect temp dirs.\n\n");
			printf("     Use these pre-injected XBat Environment Variables instead:\n");
			printf("       %%EXEDIR%%    Points to the TRUE physical directory where the compiled EXE resides.\n");
			printf("       %%RESDIR%%    Points to the directory containing all your /include assets.\n\n");
			printf("     Example inside script:\n");
			printf("       copy \"%%RESDIR%%\\config.dat\" \"%%APPDATA%%\\\"\n");
			printf("       echo Current Wrapper EXE location is: %%EXEDIR%%\n\n");

			printf("Examples:\n");
			printf("  1. Standard Mode compilation with hidden window:\n");
			printf("     %s /bat run.bat /exe output.exe /invisible\n\n", XBAT_CLI_EXE_NAME);
			printf("  2. Heavy packing with assets, custom icon, metadata injection and x64 payload:\n");
			printf("     %s /bat build.bat /exe final.exe /mode standard /x64\n", XBAT_CLI_EXE_NAME);
			printf("                   /include .\\my_assets /icon app.ico /ver version.ini\n\n");
			printf("  3. Pure in-memory high-protection pipe execution:\n");
			printf("     %s /bat core_logic.bat /exe secured.exe /mode memory\n\n", XBAT_CLI_EXE_NAME);

			printf("======================================================================\n");

			return FALSE;
		}

		else PARSE_STR_PARAM("/bat", lpOpt->szSrcBatPath, "/bat requires a file path")
		else PARSE_STR_PARAM("/exe", lpOpt->szTargetExePath, "/exe requires an output path.")
		else PARSE_STR_PARAM("/icon", lpOpt->szIconPath, "/icon requires a file path.")
		else PARSE_STR_PARAM("/ver", lpOpt->szVerInfoPath, "/ver requires a file path or '-'.")

		else if (IsArgEqual(lpszArg, "/include")) {
		if (i + 1 < argc) {
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
		else if (CheckArgPattern("/noclean")) {
					lpOpt->bDestroyRes = FALSE;
					}
		else if (CheckArgPattern("/x64")) {
						lpOpt->bUseX64 = TRUE;
						}
		else if (CheckArgPattern("/upx")) {
							lpOpt->bUseUpx = TRUE;
							}
	}

	if (lpOpt->szSrcBatPath[0] == '\0') {
		fprintf(stderr, "Error: Source batch file (/bat) is required.\n\n");
		return FALSE;
	}


	return TRUE;
}



int main(int argc, char* argv[]) {
	GetModuleFileName(NULL, g_szConverterExePath, MAX_PATH);
	_tcscpy_s(g_szConverterDirPath, MAX_PATH, g_szConverterExePath);
	PathRemoveFileSpec(g_szConverterDirPath);

	InitTempWorkDir();

	CONVERTER_OPTIONS opt = { 0 };
	static XBAT_CONFIG cfg = { 0 };
	CONVERTER_LIST lst = { 0 };

	if (!ParseCmdLine(argc, argv, &opt)) return FALSE;

	cfg.Magic = XBAT_MAGIC_INT;
	// Default config values
	cfg.GlobalFlags |= XBAT_FLAG_SELF_DESTROY;
	cfg.DropDirType = XBAT_DROP_DIR_TEMP;

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

	if (opt.bDestroyRes) cfg.GlobalFlags |= XBAT_FLAG_DESTROY_RESOURCES;
	if (opt.nDropDirType >= 0) {
		cfg.DropDirType = (UINT)opt.nDropDirType;
	}
	else {
		fprintf(stderr, "Error: Undefined drop directory type.\n");
	}

	// Load up config struct
	lst.lpConfig = &cfg;

	// Populate paths
	if (opt.szTargetExePath[0] == '\0') {
		strcpy_s(opt.szTargetExePath, MAX_PATH, opt.szSrcBatPath);
		if (!PathRenameExtensionA(opt.szTargetExePath, ".exe")) {
			fprintf(stderr, "Error: Failed to change extension.\n");
		}
	}

	if (opt.szSrcBatPath[0] != '\0') {
		CHAR2TCHAR(lst.szScriptPath, opt.szSrcBatPath, MAX_PATH);
	}

	if (opt.szTargetExePath[0] != '\0') {
		CHAR2TCHAR(lst.szOutputPath, opt.szTargetExePath, MAX_PATH);
	}

	if (opt.szIconPath[0] != '\0') {
		CHAR2TCHAR(lst.szIconPath, opt.szIconPath, MAX_PATH);
	}

	if (opt.szVerInfoPath[0] != '\0') {
		if (opt.szVerInfoPath[0] == '-') {
			HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
			if (hStdin == INVALID_HANDLE_VALUE) {
				fprintf(stderr, "Error: Failed to obtain stdin.\n");
				return FALSE;
			}

			TCHAR szTempFileName[MAX_PATH];
			GetTempFileName(g_szTempWorkDirPath, _T("XBat_Ver_"), 0, szTempFileName);

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
		}
		else {
			// Copy path directly
			CHAR2TCHAR(lst.szVerInfoPath, opt.szVerInfoPath, MAX_PATH);
		}
	}

	// Parse user resources
	std::vector<std::string> vecUserDirs;

	if (opt.bHasUserRes) {
		cfg.GlobalFlags |= XBAT_FLAG_HAS_USER_RESOURCES;

		for (size_t i = 0; i < opt.vecIncPaths.size(); ++i) {
			const char* pszPath = opt.vecIncPaths[i].c_str();
			DWORD dwAttr = GetFileAttributesA(pszPath);
			if (dwAttr == INVALID_FILE_ATTRIBUTES) continue;
			if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
				vecUserDirs.push_back(opt.vecIncPaths[i]);
			}
			else {
				// Pack single file
				XBAT_RESOURCE res = { 0 };
				CHAR2TCHAR(res.szFilePath, opt.vecIncPaths[i].data(), MAX_PATH);
				res.dwFileAttribute = GetFileAttributes(res.szFilePath);
				lst.vecResList.push_back(res);
			}
		}

		if (!vecUserDirs.empty()) {
			// Inject calling 7zdec flag
			cfg.GlobalFlags |= XBAT_FLAG_CALL_7ZDEC;

			TCHAR szTempArchivePath[MAX_PATH];
			TCHAR sz7zDecExePath[MAX_PATH];
			if (opt.bUseX64) {
				_stprintf_s(sz7zDecExePath, MAX_PATH, _T("%s\\templates\\resources\\x64\\%s"), g_szConverterDirPath, XBAT_ARCHIVE_DECODER_NAME);
			}
			else {
				_stprintf_s(sz7zDecExePath, MAX_PATH, _T("%s\\templates\\resources\\x86\\%s"), g_szConverterDirPath, XBAT_ARCHIVE_DECODER_NAME);
			}


			TCHAR sz7zPath[MAX_PATH];
			_stprintf_s(sz7zPath, MAX_PATH, _T("%s\\tools\\7zr.exe"), g_szConverterDirPath);

			_stprintf_s(szTempArchivePath, MAX_PATH, _T("%s\\%s"), g_szTempWorkDirPath, XBAT_ARCHIVE_FILE_NAME);
			_tcscpy_s(g_szTempArchivePath, MAX_PATH, szTempArchivePath);

			// Archive user folders

			TCHAR szListFilePath[MAX_PATH];
			_stprintf_s(szListFilePath, MAX_PATH, _T("%s\\pack_list.txt"), g_szTempWorkDirPath);

			// Write list file
			FILE* fpList = NULL;
			if (_tfopen_s(&fpList, szListFilePath, _T("w")) == 0 && fpList != NULL) {
				for (size_t d = 0; d < vecUserDirs.size(); ++d) {
					// Using CRLF
					fprintf(fpList, "%s\r\n", vecUserDirs[d].c_str());
				}
				fclose(fpList);
			}
			else {
				fprintf(stderr, "Error: Cannot create 7z list file.\n");
				return FALSE;
			}

			STARTUPINFO si = { sizeof(si) };
			PROCESS_INFORMATION pi = { 0 };
			si.cb = sizeof(si);

			static TCHAR szCmdLine[(MAX_PATH * 3) + 128];
			_stprintf_s(szCmdLine, _countof(szCmdLine), _T("\"%s\" a \"%s\" @\"%s\" -m0=LZMA -ms=on"), sz7zPath, szTempArchivePath, szListFilePath);

			if (CreateProcess(
				NULL,
				szCmdLine,
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

			DeleteFile(szListFilePath);


			// Add archive and decoder as resources
			XBAT_RESOURCE ArchiveRes = { 0 };
			_tcscpy_s(ArchiveRes.szFilePath, MAX_PATH, szTempArchivePath);
			ArchiveRes.dwFileAttribute = GetFileAttributes(ArchiveRes.szFilePath);
			lst.vecResList.push_back(ArchiveRes);

			XBAT_RESOURCE DecoderRes = { 0 };
			_tcscpy_s(DecoderRes.szFilePath, MAX_PATH, sz7zDecExePath);
			DecoderRes.dwFileAttribute = GetFileAttributes(DecoderRes.szFilePath);
			lst.vecResList.push_back(DecoderRes);

		}
	}

	// Construct stub path
	_stprintf_s(lst.szStubPath, MAX_PATH, _T("%s\\templates\\"), g_szConverterDirPath);
	if (opt.bUseX64) {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("x64\\"));
	}
	else {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("x86\\"));
	}
	_tcscat_s(lst.szStubPath, MAX_PATH, _T("stub"));
	if (opt.bShowConsole) {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("_cli"));
	}
	else {
		_tcscat_s(lst.szStubPath, MAX_PATH, _T("_gui"));
	}
	_tcscat_s(lst.szStubPath, MAX_PATH, _T(".bin"));
	//_stprintf_s(lst.szStubPath, MAX_PATH, _T("\"%s\""), lst.szStubPath);

	ConverterProcess(&lst);

	if (opt.bUseUpx) {
		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi = { 0 };
		si.cb = sizeof(si);

		TCHAR szUpxPath[MAX_PATH];
		_stprintf_s(szUpxPath, MAX_PATH, _T("%s\\tools\\upx.exe"), g_szConverterDirPath);

		static TCHAR szCmdLine[MAX_PATH * 2];
		_stprintf_s(szCmdLine, _countof(szCmdLine), _T("\"%s\" \"%s\""), szUpxPath, lst.szOutputPath);

		if (CreateProcess(
			NULL,
			szCmdLine,
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
	}

	DestroyTempWorkDir();

	return TRUE;
}