#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <windows.h>

#define XBAT_MAGIC_DATA {0x58,0x42,0x41,0x54} // "XBAT"
#define XBAT_MAGIC_INT 0x54414258

#define XBAT_FLAG_SHOW_CONSOLE          (1 << 0)
#define XBAT_FLAG_RUN_BAT_AS_FILE       (1 << 1)
#define XBAT_FLAG_CALL_7ZDEC			(1 << 2)
#define XBAT_FLAG_HAS_USER_RESOURCES	(1 << 3)
#define XBAT_FLAG_USE_PIPE				(1 << 4)
#define XBAT_FLAG_SELF_DESTROY			(1 << 5) // Only for debugging use, should be enabled by default
#define XBAT_FLAG_DESTROY_RESOURCES		(1 << 6) // Destroy resource at exit. Does NOT work if XBAT_FLAG_SELF_DESTROY not enabled.

typedef enum {
	MODE_FATIH = 0,
	MODE_MEMORY,
	MODE_LITE
} XBAT_MODE;

#define XBAT_DROP_DIR_TEMP		0
#define XBAT_DROP_DIR_CURR		1

#define XBAT_FINAL_KEY_LENGTH 16
#define XBAT_RES_FILE_NAME_LENGTH 64
#define XBAT_CONSOLE_TITLE_LENGTH 64

// Res
#define IDR_XBAT_KEY 998
#define IDR_XBAT_BAT 500
#define IDR_XBAT_CONFIG 999
#define IDR_XBAT_USER_RES_START IDR_XBAT_BAT + 1
#define IDR_XBAT_USER_RES_END   IDR_XBAT_CONFIG - 99
#define IDI_XBAT 2


#define XBAT_KEY_OBFUSCATOR 0xA5

#pragma pack(push, 1)
typedef struct {
	UINT Magic;   // unsigned int as 4 bytes
	UINT SavedCrc;
	DWORD dwOriginalSize;
	DWORD dwAttributes;
	TCHAR szFileName[XBAT_RES_FILE_NAME_LENGTH];
	BYTE Data[1];
} XBAT_RES_HEADER;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	UINT Magic;
	UINT GlobalFlags;
	UINT DropDirType;
	CHAR szConsoleTitle[XBAT_CONSOLE_TITLE_LENGTH]; // ANSI for console
} XBAT_CONFIG;
#pragma pack(pop)

// String conversion macro
#ifdef UNICODE
#define CHAR2TCHAR(dest, src, len) MultiByteToWideChar(CP_ACP, 0, (src), -1, (dest), (len))
#define TCHAR2CHAR(dest, src, len) WideCharToMultiByte(CP_ACP, 0, (src), -1, (dest), (len), NULL, NULL)
#else
#define CHAR2TCHAR(dest, src, len) strcpy_s((dest), (len), (src))
#define TCHAR2CHAR(dest, src, len) strcpy_s((dest), (len), (src))
#endif

#define XBAT_STUB_ARCH_X86 0
#define XBAT_STUB_ARCH_X64 1

typedef struct {
	TCHAR szComments[128];
	TCHAR szCompanyName[128];
	TCHAR szFileDescription[128];
	TCHAR szFileVersion[128];
	TCHAR szInternalName[128];
	TCHAR szLegalCopyright[128];
	TCHAR szLegalTrademarks[128];
	TCHAR szOriginalFilename[128];
	TCHAR szPrivateBuild[128];
	TCHAR szProductName[128];
	TCHAR szProductVersion[128];
	TCHAR szSpecialBuild[128];
} STUB_VERSION_INFO;

#define STDIN_BUF_SIZE 4096

#define XBAT_ARCHIVE_FILE_NAME TEXT("dir_pack.7z")
#define XBAT_ARCHIVE_DECODER_NAME TEXT("7zdec.exe")

#endif
