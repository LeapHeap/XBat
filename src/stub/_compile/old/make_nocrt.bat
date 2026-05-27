@echo off
setlocal

:: ============================================================
:: 1. Public Path Configuration (Modify here to sync all tasks)
:: ============================================================
set "XBAT_ROOT=F:\_lzy_files\lh_codes\XBat"
set "XBAT_COMMON_PATH=%XBAT_ROOT%\src\common"
set "STUB_ROOT_PATH=%XBAT_ROOT%\src\stub"

:: Compiler Paths
set "MINGW_BIN=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "VC6_ROOT=D:\DevTools\VC6Portable\VC98"
set "VC6_COMMON_BIN=D:\DevTools\VC6Portable\Common\MSDev98\Bin"

:: Output Directories
set "OUT_X64=%XBAT_ROOT%\bin\x64\templates\x64"
set "OUT_X86=%XBAT_ROOT%\bin\x64\templates\x86"

:: Source Sub-paths
set "LZMADEC_PATH=%XBAT_COMMON_PATH%\lzma_sdk\lzmadec"
set "STUB_FULL_PATH=%STUB_ROOT_PATH%\stub_full"

:: Custom Entry Point (确保你的源码入口函数名为 MyMain，且包裹了 extern "C")
set "ENTRY_NAME=MyMain"

:: ============================================================
:: 2. Execute Compilation Process
:: ============================================================

echo ============================================================
echo Starting All No-CRT Dual-Mode Compilation Tasks...
echo ============================================================

:: --- Task 1: x64 Full GUI (MinGW) ---
call :compile_mingw "Full_GUI" "stub_full_gui.bin" "-DMODE_FULL" "windows"

:: --- Task 2: x64 Full CLI (MinGW) ---
call :compile_mingw "Full_CLI" "stub_full_cli.bin" "-DMODE_FULL -DMODE_CLI" "console"

:: --- Task 3: x64 Lite (MinGW) ---
call :compile_mingw "Lite" "stub_lite.bin" "-DBUILDING_LITE" "windows"


:: --- Task 4: x86 Full GUI (VC98) ---
call :compile_vc6 "Full_GUI" "stub_full_gui.bin" "/DMODE_FULL" "WINDOWS"

:: --- Task 5: x86 Full CLI (VC98) ---
call :compile_vc6 "Full_CLI" "stub_full_cli.bin" "/DMODE_FULL /DMODE_CLI" "CONSOLE"

:: --- Task 6: x86 Lite (VC98) ---
call :compile_vc6 "Lite" "stub_lite.bin" "/DBUILDING_LITE" "WINDOWS"


echo.
echo ============================================================
echo All compilations finished successfully (Zero CRT Pollution)!
echo ============================================================
pause
exit /b

:: ============================================================
:: Function: MinGW Compilation Logic (x64)
:: ============================================================
:compile_mingw
echo [*] Compiling x64 %~1 (MinGW)...
setlocal
set "PATH=%MINGW_BIN%;%PATH%"
if not exist "%OUT_X64%" mkdir "%OUT_X64%"

set "CFLAGS=-Os -pipe -ffreestanding -mno-stack-arg-probe -fno-ms-extensions -DUNICODE -D_UNICODE %~3"

set "LDFLAGS=-m%~4 -nostdlib -e %ENTRY_NAME% -Wl,--gc-sections -s"

:: Compile Modules
gcc %CFLAGS% -c "%STUB_ROOT_PATH%\stub_main.c" -o stub_main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

if "%~1"=="Full_GUI" (
    gcc %CFLAGS% -c "%LZMADEC_PATH%\LzmaDec.c" -o LzmaDec.o
    gcc %CFLAGS% -c "%STUB_FULL_PATH%\stub_full.c" -o stub_full.o
    gcc -o "%OUT_X64%\%~2" stub_main.o stub_full.o crypto.o Utils.o LzmaDec.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
) else if "%~1"=="Full_CLI" (
    gcc %CFLAGS% -c "%LZMADEC_PATH%\LzmaDec.c" -o LzmaDec.o
    gcc %CFLAGS% -c "%STUB_FULL_PATH%\stub_full.c" -o stub_full.o
    gcc -o "%OUT_X64%\%~2" stub_main.o stub_full.o crypto.o Utils.o LzmaDec.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
) else (
    gcc -o "%OUT_X64%\%~2" stub_main.o crypto.o Utils.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
)

:: Clean temporary object files
del /f /q *.o
endlocal
exit /b

:: ============================================================
:: Function: VC6 Compilation Logic (x86)
:: ============================================================
:compile_vc6
echo [*] Compiling x86 %~1 (VC98)...
setlocal
set "PATH=%VC6_ROOT%\Bin;%VC6_COMMON_BIN%;%PATH%"
set "INCLUDE=%VC6_ROOT%\Include;%INCLUDE%"
set "LIB=%VC6_ROOT%\Lib;%LIB%"
if not exist "%OUT_X86%" mkdir "%OUT_X86%"

set "COMMON_FLAGS=/TP /GX- /GR- /Gs999999 /DMODE_VC6 /DUNICODE /D_UNICODE %~3"

:: Compile Modules 
cl.exe /c /O1 /nologo "%STUB_ROOT_PATH%\stub_main.c" %COMMON_FLAGS%
cl.exe /c /O1 /nologo "%XBAT_COMMON_PATH%\crypto.c" %COMMON_FLAGS%
cl.exe /c /O1 /nologo "%XBAT_COMMON_PATH%\Utils.c" %COMMON_FLAGS%

set "LINK_FLAGS=/nologo /NODEFAULTLIB /ENTRY:%ENTRY_NAME% /OPT:REF /OPT:ICF kernel32.lib shell32.lib user32.lib shlwapi.lib /SUBSYSTEM:%~4"

if "%~1"=="Full_GUI" (
    cl.exe /c /O1 /nologo "%LZMADEC_PATH%\LzmaDec.c" %COMMON_FLAGS%
    cl.exe /c /O1 /nologo "%STUB_FULL_PATH%\stub_full.c" %COMMON_FLAGS%
    link.exe LzmaDec.obj stub_main.obj stub_full.obj crypto.obj Utils.obj /OUT:"%OUT_X86%\%~2" %LINK_FLAGS%
) else if "%~1"=="Full_CLI" (
    cl.exe /c /O1 /nologo "%LZMADEC_PATH%\LzmaDec.c" %COMMON_FLAGS%
    cl.exe /c /O1 /nologo "%STUB_FULL_PATH%\stub_full.c" %COMMON_FLAGS%
    link.exe LzmaDec.obj stub_main.obj stub_full.obj crypto.obj Utils.obj /OUT:"%OUT_X86%\%~2" %LINK_FLAGS%
) else (
    link.exe stub_main.obj crypto.obj Utils.obj /OUT:"%OUT_X86%\%~2" %LINK_FLAGS%
)

:: Clean temporary object files
del /s /f /q *.obj
endlocal
exit /b