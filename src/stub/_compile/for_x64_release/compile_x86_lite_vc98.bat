@echo off
set PATH=D:\DevTools\VC6Portable\VC98\Bin;%PATH%
set PATH=D:\DevTools\VC6Portable\Common\MSDev98\Bin;%PATH%
set INCLUDE=D:\DevTools\VC6Portable\VC98\Include;%INCLUDE%
set LIB=D:\DevTools\VC6Portable\VC98\Lib;%LIB%

set STUB_ROOT_PATH=F:\_lzy_files\lh_codes\XBat\src\stub
set XBAT_COMMON_PATH=F:\_lzy_files\lh_codes\XBat\src\common
set STUB_X86_OUTPUT_PATH=F:\_lzy_files\lh_codes\XBat\bin\x64\templates\x86

set "COMMON_FLAGS=/TP /GX- /GR- /DMODE_VC6 /DUNICODE /D_UNICODE"

:: Compile (Using C++ mode)
cl.exe /c /O1 /MD /nologo %STUB_ROOT_PATH%\main.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %XBAT_COMMON_PATH%\crypto.c %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo %XBAT_COMMON_PATH%\Utils.c %COMMON_FLAGS%

::Link
link.exe main.obj crypto.obj Utils.obj /OUT:%STUB_X86_OUTPUT_PATH%\stub_lite.bin /nologo /OPT:REF /OPT:ICF kernel32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS

::Clean
del /s /f /q *.obj