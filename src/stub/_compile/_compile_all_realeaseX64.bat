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

:: ============================================================
:: 2. Execute Compilation Process
:: ============================================================

echo ========================================
echo Starting All Compilation Tasks...
echo ========================================

:: --- Task 1: x64 Full (MinGW) ---
call :compile_mingw "Full" "stub_full.bin" "-DMODE_FULL"

:: --- Task 2: x64 Lite (MinGW) ---
call :compile_mingw "Lite" "stub_lite.bin" "-DBUILDING_LITE"

:: --- Task 3: x86 Full (VC98) ---
call :compile_vc6 "Full" "stub_full.bin" "/DMODE_FULL"

:: --- Task 4: x86 Lite (VC98) ---
call :compile_vc6 "Lite" "stub_lite.bin" ""

echo.
echo ========================================
echo All compilations are finished!
echo ========================================
pause
exit /b

:: ============================================================
:: Function: MinGW Compilation Logic
:: ============================================================
:compile_mingw
echo [*] Compiling x64 %~1 (MinGW)...
setlocal
set "PATH=%MINGW_BIN%;%PATH%"
if not exist "%OUT_X64%" mkdir "%OUT_X64%"

set "CFLAGS=-Os -pipe -fno-ms-extensions -DUNICODE -D_UNICODE %~3"
set "LDFLAGS=-mwindows -Wl,--gc-sections -static -s"

:: Compile Modules
gcc %CFLAGS% -c "%STUB_ROOT_PATH%\stub_main.c" -o stub_main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

if "%~1"=="Full" (
    gcc %CFLAGS% -c "%LZMADEC_PATH%\LzmaDec.c" -o LzmaDec.o
    gcc %CFLAGS% -c "%STUB_FULL_PATH%\stub_full.c" -o stub_full.o
    gcc -o "%OUT_X64%\%~2" stub_main.o stub_full.o crypto.o Utils.o LzmaDec.o %LDFLAGS% -lkernel32 -lshell32 -luser32
) else (
    gcc -o "%OUT_X64%\%~2" stub_main.o crypto.o Utils.o %LDFLAGS% -lkernel32 -lshell32 -luser32
)

:: Clean temporary object files
del /f /q *.o
endlocal
exit /b

:: ============================================================
:: Function: VC6 Compilation Logic
:: ============================================================
:compile_vc6
echo [*] Compiling x86 %~1 (VC98)...
setlocal
set "PATH=%VC6_ROOT%\Bin;%VC6_COMMON_BIN%;%PATH%"
set "INCLUDE=%VC6_ROOT%\Include;%INCLUDE%"
set "LIB=%VC6_ROOT%\Lib;%LIB%"
if not exist "%OUT_X86%" mkdir "%OUT_X86%"

set "COMMON_FLAGS=/TP /GX- /GR- /DMODE_VC6 /DUNICODE /D_UNICODE %~3"

:: Compile Modules (Using C++ mode for better strictness)
cl.exe /c /O1 /MD /nologo "%STUB_ROOT_PATH%\stub_main.c" %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo "%XBAT_COMMON_PATH%\crypto.c" %COMMON_FLAGS%
cl.exe /c /O1 /MD /nologo "%XBAT_COMMON_PATH%\Utils.c" %COMMON_FLAGS%

if "%~1"=="Full" (
    cl.exe /c /O1 /MD /nologo "%LZMADEC_PATH%\LzmaDec.c" %COMMON_FLAGS%
    cl.exe /c /O1 /MD /nologo "%STUB_FULL_PATH%\stub_full.c" %COMMON_FLAGS%
    link.exe LzmaDec.obj stub_main.obj stub_full.obj crypto.obj Utils.obj /OUT:"%OUT_X86%\%~2" /nologo /OPT:REF /OPT:ICF kernel32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS
) else (
    link.exe stub_main.obj crypto.obj Utils.obj /OUT:"%OUT_X86%\%~2" /nologo /OPT:REF /OPT:ICF kernel32.lib shell32.lib user32.lib /SUBSYSTEM:WINDOWS
)

:: Clean temporary object files
del /s /f /q *.obj
endlocal
exit /b