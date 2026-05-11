@echo off
set PATH=D:\DevTools\VC6Portable\VC98\Bin;%PATH%
set PATH=D:\DevTools\VC6Portable\Common\MSDev98\Bin;%PATH%
set INCLUDE=D:\DevTools\VC6Portable\VC98\Include;%INCLUDE%
set LIB=D:\DevTools\VC6Portable\VC98\Lib;%LIB%

set LZMADEC_PATH=F:\_lzy_files\lh_codes\XBat\src\common\lzma_sdk\lzmadec
set STUB_ROOT_PATH=F:\_lzy_files\lh_codes\XBat\src\stub
set STUB_FULL_PATH=%STUB_ROOT_PATH%\stub_full
set XBAT_COMMON_PATH=F:\_lzy_files\lh_codes\XBat\src\common
set STUB_X86_OUTPUT_PATH=F:\_lzy_files\lh_codes\XBat\bin\x64\templates\x86

set "COMMON_FLAGS=/TP /GX- /GR- /DMODE_VC6 /DUNICODE /D_UNICODE /DMODE_FULL"

:: Compile (Using C++ mode)
cl.exe /c /O1 /MD /nologo %LZMADEC_PATH%\LzmaDec.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %STUB_ROOT_PATH%\main.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %STUB_FULL_PATH%\stub_full.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %XBAT_COMMON_PATH%\crypto.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %XBAT_COMMON_PATH%\Utils.c %COMMON_FLAGS%

::Link
link.exe LzmaDec.obj main.obj stub_full.obj crypto.obj Utils.obj /OUT:%STUB_X86_OUTPUT_PATH%\stub_full.bin /nologo /OPT:REF /OPT:ICF kernel32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS

::Clean
del /s /f /q crypto.obj LzmaDec.obj main.obj stub_full.obj Utils.obj