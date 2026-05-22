#pragma once

#include <windows.h>
#include "../common/shared_defs.h"

BOOL
InjectResIntoExe(
    LPCTSTR lpszExePath,
    LPCTSTR lpszResPath
);

BOOL
BuildResourceFile(
    LPCTSTR             lpszIconPath,
    STUB_VERSION_INFO* lpVerInfo,
    LPTSTR              lpszOutResPath,
    DWORD               cchOutResPath,
    BOOL                bHasVersionInfo
);