/*=========================================================================
    restool.c
=========================================================================*/

#define WIN32_LEAN_AND_MEAN

#include "restool.h"
#include "../common/shared_defs.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#ifndef _countof
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

typedef struct _RESHEADER
{
    DWORD dwDataSize;
    DWORD dwHeaderSize;

} RESHEADER;

typedef struct _RESIDENTIFIER
{
    BOOL    bIsInt;
    WORD    wId;
    LPCTSTR lpszName;

} RESIDENTIFIER;

#define DWORD_ALIGN(_x) (((_x) + 3) & ~3)

/*=========================================================================
    Escape RC string
=========================================================================*/

static VOID
RcEscapeString(
    LPCTSTR lpszInput,
    LPTSTR  lpszOutput,
    SIZE_T  cchOutput
)
{
    SIZE_T i;
    SIZE_T j;

    if (!lpszInput || !lpszOutput || cchOutput == 0)
    {
        return;
    }

    i = 0;
    j = 0;

    while (lpszInput[i] != 0)
    {
        if (j + 2 >= cchOutput)
        {
            break;
        }

        if (lpszInput[i] == _T('\\') ||
            lpszInput[i] == _T('\"'))
        {
            lpszOutput[j++] = _T('\\');
        }

        lpszOutput[j++] = lpszInput[i++];
    }

    lpszOutput[j] = 0;
}

/*=========================================================================
    Read resource identifier
=========================================================================*/

static BOOL
ResReadIdentifier(
    const BYTE** lplpbData,
    const BYTE* lpbEnd,
    RESIDENTIFIER* lpIdent
)
{
    const WORD* lpwData;

    if (!lplpbData || !*lplpbData || !lpIdent)
    {
        return FALSE;
    }

    if ((*lplpbData + sizeof(WORD)) > lpbEnd)
    {
        return FALSE;
    }

    lpwData = (const WORD*)(*lplpbData);

    //
    // Integer ID
    //
    if (lpwData[0] == 0xFFFF)
    {
        if ((*lplpbData + 4) > lpbEnd)
        {
            return FALSE;
        }

        lpIdent->bIsInt = TRUE;
        lpIdent->wId = lpwData[1];
        lpIdent->lpszName = NULL;

        *lplpbData += 4;
    }
    else
    {
        LPCTSTR lpszStr;
        SIZE_T  cchLen;
        SIZE_T  cbLen;

        lpszStr = (LPCTSTR)(*lplpbData);

        cchLen = 0;

        while (TRUE)
        {
            if (((const BYTE*)(lpszStr + cchLen + 1)) > lpbEnd)
            {
                return FALSE;
            }

            if (lpszStr[cchLen] == _T('\0'))
            {
                break;
            }

            cchLen++;
        }

        cbLen = (cchLen + 1) * sizeof(TCHAR);

        lpIdent->bIsInt = FALSE;
        lpIdent->wId = 0;
        lpIdent->lpszName = lpszStr;

        *lplpbData += cbLen;
    }

    return TRUE;
}

/*=========================================================================
    Convert identifier
=========================================================================*/

static LPCTSTR
ResToTString(
    const RESIDENTIFIER* lpIdent
)
{
    if (lpIdent->bIsInt)
    {
        return MAKEINTRESOURCE(lpIdent->wId);
    }

    return lpIdent->lpszName;
}

/*=========================================================================
    Inject .res into PE
=========================================================================*/

BOOL
InjectResIntoExe(
    LPCTSTR lpszExePath,
    LPCTSTR lpszResPath
)
{
    HANDLE  hFile;
    HANDLE  hHeap;
    HANDLE  hUpdate;

    DWORD   dwFileSize;
    DWORD   dwBytesRead;

    BYTE* lpbResData;
    BYTE* lpbCur;
    BYTE* lpbEnd;

    BOOL    bSuccess;

    hFile = INVALID_HANDLE_VALUE;
    hUpdate = NULL;
    lpbResData = NULL;
    bSuccess = FALSE;

    hHeap = GetProcessHeap();

    hFile = CreateFile(
        lpszResPath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        _tprintf(_T("CreateFile failed: %lu\n"), GetLastError());
        goto Cleanup;
    }

    dwFileSize = GetFileSize(hFile, NULL);

    if (dwFileSize == INVALID_FILE_SIZE ||
        dwFileSize == 0)
    {
        _tprintf(_T("Invalid .res size.\n"));
        goto Cleanup;
    }

    lpbResData = (BYTE*)HeapAlloc(
        hHeap,
        0,
        dwFileSize);

    if (!lpbResData)
    {
        _tprintf(_T("HeapAlloc failed.\n"));
        goto Cleanup;
    }

    if (!ReadFile(
        hFile,
        lpbResData,
        dwFileSize,
        &dwBytesRead,
        NULL))
    {
        _tprintf(_T("ReadFile failed: %lu\n"), GetLastError());
        goto Cleanup;
    }

    CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;

    if (dwBytesRead != dwFileSize)
    {
        _tprintf(_T("Incomplete file read.\n"));
        goto Cleanup;
    }

    hUpdate = BeginUpdateResource(
        lpszExePath,
        FALSE);

    if (!hUpdate)
    {
        _tprintf(
            _T("BeginUpdateResource failed: %lu\n"),
            GetLastError());

        goto Cleanup;
    }

    lpbCur = lpbResData;
    lpbEnd = lpbResData + dwFileSize;

    while (lpbCur < lpbEnd)
    {
        RESHEADER* lpHeader;

        BYTE* lpbHeaderEnd;
        BYTE* lpbRawData;

        RESIDENTIFIER  TypeIdent;
        RESIDENTIFIER  NameIdent;

        DWORD          dwDataVersion;
        WORD           wMemoryFlags;
        WORD           wLanguageId;
        DWORD          dwVersion;
        DWORD          dwCharacteristics;

        LPCTSTR        lpszType;
        LPCTSTR        lpszName;

        if ((lpbCur + sizeof(RESHEADER)) > lpbEnd)
        {
            _tprintf(_T("Invalid RES header.\n"));
            goto Cleanup;
        }

        lpHeader = (RESHEADER*)lpbCur;

        //
        // Skip dummy entry
        //
        if (lpHeader->dwDataSize == 0)
        {
            lpbCur += lpHeader->dwHeaderSize;
            continue;
        }

        lpbHeaderEnd = lpbCur + lpHeader->dwHeaderSize;

        if (lpbHeaderEnd > lpbEnd)
        {
            _tprintf(_T("Corrupted header.\n"));
            goto Cleanup;
        }

        lpbCur += sizeof(RESHEADER);

        if (!ResReadIdentifier(
            (const BYTE**)&lpbCur,
            lpbHeaderEnd,
            &TypeIdent))
        {
            _tprintf(_T("Type parse failed.\n"));
            goto Cleanup;
        }

        if (!ResReadIdentifier(
            (const BYTE**)&lpbCur,
            lpbHeaderEnd,
            &NameIdent))
        {
            _tprintf(_T("Name parse failed.\n"));
            goto Cleanup;
        }

        lpbCur = (BYTE*)DWORD_ALIGN((ULONG_PTR)lpbCur);

        if ((lpbCur + 16) > lpbHeaderEnd)
        {
            _tprintf(_T("Corrupted extra header.\n"));
            goto Cleanup;
        }

        dwDataVersion = *(DWORD*)lpbCur; lpbCur += 4;
        wMemoryFlags = *(WORD*)lpbCur;  lpbCur += 2;
        wLanguageId = *(WORD*)lpbCur;  lpbCur += 2;
        dwVersion = *(DWORD*)lpbCur; lpbCur += 4;
        dwCharacteristics = *(DWORD*)lpbCur; lpbCur += 4;

        UNREFERENCED_PARAMETER(dwDataVersion);
        UNREFERENCED_PARAMETER(wMemoryFlags);
        UNREFERENCED_PARAMETER(dwVersion);
        UNREFERENCED_PARAMETER(dwCharacteristics);

        lpszType = ResToTString(&TypeIdent);
        lpszName = ResToTString(&NameIdent);

        lpbRawData =
            ((BYTE*)lpHeader) + lpHeader->dwHeaderSize;

        if ((lpbRawData + lpHeader->dwDataSize) > lpbEnd)
        {
            _tprintf(_T("Corrupted resource data.\n"));
            goto Cleanup;
        }

        if (!UpdateResource(
            hUpdate,
            (LPTSTR)lpszType,
            (LPTSTR)lpszName,
            wLanguageId,
            lpbRawData,
            lpHeader->dwDataSize))
        {
            _tprintf(
                _T("UpdateResource failed: %lu\n"),
                GetLastError());

            goto Cleanup;
        }

        lpbCur =
            lpbRawData +
            DWORD_ALIGN(lpHeader->dwDataSize);
    }

    if (!EndUpdateResource(hUpdate, FALSE))
    {
        _tprintf(
            _T("EndUpdateResource failed: %lu\n"),
            GetLastError());

        goto Cleanup;
    }

    hUpdate = NULL;

    bSuccess = TRUE;

Cleanup:

    if (hUpdate)
    {
        EndUpdateResource(hUpdate, TRUE);
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
    }

    if (lpbResData)
    {
        HeapFree(hHeap, 0, lpbResData);
    }

    return bSuccess;
}

/*=========================================================================
    Build RC + RES
=========================================================================*/

BOOL
BuildResourceFile(
    LPCTSTR             lpszIconPath,
    STUB_VERSION_INFO* lpVerInfo,
    LPTSTR              lpszOutResPath,
    DWORD               cchOutResPath,
    BOOL                bHasVersionInfo
)
{
    TCHAR   szTempPath[MAX_PATH];
    TCHAR   szTempRcPath[MAX_PATH];
    TCHAR   szTempResPath[MAX_PATH];

    TCHAR   szCmdLine[1024];

    TCHAR   szEscaped[512];

    LPTSTR  lpszExt;

    FILE* fpRc;

    STARTUPINFO         si;
    PROCESS_INFORMATION pi;

    DWORD   dwExitCode;

    BOOL    bSuccess;

    fpRc = NULL;
    bSuccess = FALSE;

    if (!GetTempPath(
        _countof(szTempPath),
        szTempPath))
    {
        return FALSE;
    }

    if (!GetTempFileName(
        szTempPath,
        _T("RC"),
        0,
        szTempRcPath))
    {
        return FALSE;
    }

    lpszExt = _tcsrchr(szTempRcPath, _T('.'));

    if (!lpszExt)
    {
        goto Cleanup;
    }

    _tcscpy_s(
        lpszExt,
        MAX_PATH - (lpszExt - szTempRcPath),
        _T(".rc"));

    _tcscpy_s(
        szTempResPath,
        _countof(szTempResPath),
        szTempRcPath);

    lpszExt = _tcsrchr(szTempResPath, _T('.'));

    if (!lpszExt)
    {
        goto Cleanup;
    }

    _tcscpy_s(
        lpszExt,
        MAX_PATH - (lpszExt - szTempResPath),
        _T(".res"));

    _tfopen_s(
        &fpRc,
        szTempRcPath,
        _T("wt"));

    if (!fpRc)
    {
        goto Cleanup;
    }

    //
    // ICON
    //
    if (lpszIconPath &&
        lpszIconPath[0] != 0)
    {
        _ftprintf(
            fpRc,
            _T("32 ICON \"%s\"\n\n"),
            lpszIconPath);
    }

    //
    // VERSIONINFO
    //
    if (bHasVersionInfo && lpVerInfo)
    {
        _ftprintf(fpRc, _T("LANGUAGE 0, 0\n\n"));

        _ftprintf(fpRc, _T("1 VERSIONINFO\n"));
        _ftprintf(fpRc, _T("FILEVERSION 0,0,0,0\n"));
        _ftprintf(fpRc, _T("PRODUCTVERSION 0,0,0,0\n"));
        _ftprintf(fpRc, _T("FILEOS 0\n"));
        _ftprintf(fpRc, _T("FILETYPE 1\n"));
        _ftprintf(fpRc, _T("FILESUBTYPE 0\n"));
        _ftprintf(fpRc, _T("FILEFLAGSMASK 0\n"));
        _ftprintf(fpRc, _T("FILEFLAGS 0\n"));
        _ftprintf(fpRc, _T("{\n"));

        _ftprintf(fpRc, _T("BLOCK \"StringFileInfo\"\n"));
        _ftprintf(fpRc, _T("{\n"));

        _ftprintf(fpRc, _T("BLOCK \"04000025\"\n"));
        _ftprintf(fpRc, _T("{\n"));

#define WRITE_VER_VALUE(_field, _name)                    \
    RcEscapeString(                                       \
        lpVerInfo->_field,                                \
        szEscaped,                                        \
        _countof(szEscaped));                             \
                                                          \
    _ftprintf(                                            \
        fpRc,                                             \
        _T("VALUE \"%s\", \"%s\"\n"),                     \
        _T(_name),                                        \
        szEscaped);

        WRITE_VER_VALUE(szComments, "Comments");
        WRITE_VER_VALUE(szCompanyName, "CompanyName");
        WRITE_VER_VALUE(szFileDescription, "FileDescription");
        WRITE_VER_VALUE(szFileVersion, "FileVersion");
        WRITE_VER_VALUE(szInternalName, "InternalName");
        WRITE_VER_VALUE(szLegalCopyright, "LegalCopyright");
        WRITE_VER_VALUE(szLegalTrademarks, "LegalTrademarks");
        WRITE_VER_VALUE(szOriginalFilename, "OriginalFilename");
        WRITE_VER_VALUE(szPrivateBuild, "PrivateBuild");
        WRITE_VER_VALUE(szProductName, "ProductName");
        WRITE_VER_VALUE(szProductVersion, "ProductVersion");
        WRITE_VER_VALUE(szSpecialBuild, "SpecialBuild");

#undef WRITE_VER_VALUE

        _ftprintf(fpRc, _T("}\n"));
        _ftprintf(fpRc, _T("}\n"));

        _ftprintf(fpRc, _T("BLOCK \"VarFileInfo\"\n"));
        _ftprintf(fpRc, _T("{\n"));
        _ftprintf(
            fpRc,
            _T("VALUE \"Translation\", 0x0400, 0x0025\n"));
        _ftprintf(fpRc, _T("}\n"));

        _ftprintf(fpRc, _T("}\n"));
    }

    fclose(fpRc);
    fpRc = NULL;

    //
    // Run GoRC
    //
    _stprintf_s(
        szCmdLine,
        _countof(szCmdLine),
        _T("\"tools\\GoRC.exe\" /r \"%s\""),
        szTempRcPath);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb = sizeof(si);

    if (!CreateProcess(
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
        goto Cleanup;
    }

    WaitForSingleObject(
        pi.hProcess,
        INFINITE);

    GetExitCodeProcess(
        pi.hProcess,
        &dwExitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (dwExitCode != 0)
    {
        goto Cleanup;
    }

    _tcsncpy_s(
        lpszOutResPath,
        cchOutResPath,
        szTempResPath,
        _TRUNCATE);

    bSuccess = TRUE;

Cleanup:

    if (fpRc)
    {
        fclose(fpRc);
    }

    if (!bSuccess)
    {
        DeleteFile(szTempRcPath);
        DeleteFile(szTempResPath);
    }

    return bSuccess;
}