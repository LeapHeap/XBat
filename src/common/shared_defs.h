#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <windows.h>

#define XBAT_MAGIC_DATA {0x58,0x42,0x41,0x54} // "XBAT"
#define XBAT_MAGIC_INT 0x54414258

#define XBAT_FLAG_SHOW_CONSOLE          (1 << 0)
#define XBAT_FLAG_RUN_BAT_AS_FILE       (1 << 1)
#define XBAT_FLAG_LZMA_COMPRESSED		(1 << 2)
#define XBAT_FLAG_HAS_USER_RESOURCES	(1 << 3)
#define XBAT_FLAG_USE_PIPE				(1 << 4)

#define XBAT_DROP_DIR_TEMP		0
#define XBAT_DROP_DIR_CURR		1

#define XBAT_FINAL_KEY_LENGTH 16
#define XBAT_RES_FILE_NAME_LENGTH 64
#define XBAT_CONSOLE_TITLE_LENGTH 64

// Res
#define IDR_XBAT_KEY 998
#define IDR_XBAT_BAT 500
#define IDR_XBAT_CONFIG 999


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


#endif
