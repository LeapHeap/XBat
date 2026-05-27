@echo off
setlocal

:: ============================================================
:: 1. Public Path Configuration (Modify here to sync all tasks)
:: ============================================================
set "XBAT_ROOT=F:\_lzy_files\lh_codes\XBat"
set "XBAT_COMMON_PATH=%XBAT_ROOT%\src\common"
set "STUB_ROOT_PATH=%XBAT_ROOT%\src\stub"

:: Compiler Paths
set "MINGW64_BIN=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "MINGW32_BIN=C:\Program Files\RedPanda-Cpp\mingw32\bin"

:: Output Directories
set "OUT_X64=%XBAT_ROOT%\bin\x64\templates\x64"
set "OUT_X86=%XBAT_ROOT%\bin\x64\templates\x86"

:: Source Sub-paths
set "LZMADEC_PATH=%XBAT_COMMON_PATH%\lzma_sdk\lzmadec"
set "STUB_FULL_PATH=%STUB_ROOT_PATH%\stub_full"

:: Custom Entry Point
set "ENTRY_NAME=MyMain"

:: ============================================================
:: 2. Execute Compilation Process
:: ============================================================

echo ============================================================
echo Starting All No-CRT Dual-Mode Compilation Tasks (Pure MinGW)...
echo ============================================================

:: --- Task 1: x64 Full GUI (MinGW) ---
call :compile_mingw_x64 "Full_GUI" "stub_full_gui.bin" "-DMODE_FULL" "windows"

:: --- Task 2: x64 Full CLI (MinGW) ---
call :compile_mingw_x64 "Full_CLI" "stub_full_cli.bin" "-DMODE_FULL -DMODE_CLI" "console"

:: --- Task 3: x64 Lite (MinGW) ---
call :compile_mingw_x64 "Lite" "stub_lite.bin" "-DBUILDING_LITE" "windows"


:: --- Task 4: x86 Full GUI (MinGW 32-bit Upgraded!) ---
call :compile_mingw_x86 "Full_GUI" "stub_full_gui.bin" "-DMODE_FULL" "windows"

:: --- Task 5: x86 Full CLI (MinGW 32-bit Upgraded!) ---
call :compile_mingw_x86 "Full_CLI" "stub_full_cli.bin" "-DMODE_FULL -DMODE_CLI" "console"

:: --- Task 6: x86 Lite (MinGW 32-bit Upgraded!) ---
call :compile_mingw_x86 "Lite" "stub_lite.bin" "-DBUILDING_LITE" "windows"


echo.
echo ============================================================
echo All compilations finished successfully (Zero CRT Pollution)!
echo ============================================================
pause
exit /b

:: ============================================================
:: Function: MinGW Compilation Logic (x64)
:: ============================================================
:compile_mingw_x64
echo [*] Compiling x64 %~1 (MinGW)...
setlocal
set "PATH=%MINGW64_BIN%;%PATH%"
if not exist "%OUT_X64%" mkdir "%OUT_X64%"

set "CFLAGS=-Os -pipe -ffreestanding -mno-stack-arg-probe -fno-ms-extensions -ffunction-sections -fdata-sections -fno-ident -fno-asynchronous-unwind-tables -DUNICODE -D_UNICODE %~3"
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

del /f /q *.o
endlocal
exit /b

:: ============================================================
:: Function: MinGW Compilation Logic (x86 Upgraded)
:: ============================================================
:compile_mingw_x86
echo [*] Compiling x86 %~1 (MinGW32)...
setlocal
set "PATH=%MINGW32_BIN%;%PATH%"
if not exist "%OUT_X86%" mkdir "%OUT_X86%"

set "CFLAGS=-m32 -Os -pipe -ffreestanding -mno-stack-arg-probe -fno-ms-extensions -ffunction-sections -fdata-sections -fno-ident -fno-asynchronous-unwind-tables -DUNICODE -D_UNICODE %~3"
:: Using _%ENTRY_NAME%@0 for x86 entries
set "LDFLAGS=-m%~4 -nostdlib -e _%ENTRY_NAME%@0 -Wl,--gc-sections -s"

:: Compile Modules
gcc %CFLAGS% -c "%STUB_MAIN_PATH%\..\stub_main.c" -o stub_main.o 2>nul
if errorlevel 1 gcc %CFLAGS% -c "%STUB_ROOT_PATH%\stub_main.c" -o stub_main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

if "%~1"=="Full_GUI" (
    gcc %CFLAGS% -c "%LZMADEC_PATH%\LzmaDec.c" -o LzmaDec.o
    gcc %CFLAGS% -c "%STUB_FULL_PATH%\stub_full.c" -o stub_full.o
    gcc -o "%OUT_X86%\%~2" stub_main.o stub_full.o crypto.o Utils.o LzmaDec.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
) else if "%~1"=="Full_CLI" (
    gcc %CFLAGS% -c "%LZMADEC_PATH%\LzmaDec.c" -o LzmaDec.o
    gcc %CFLAGS% -c "%STUB_FULL_PATH%\stub_full.c" -o stub_full.o
    gcc -o "%OUT_X86%\%~2" stub_main.o stub_full.o crypto.o Utils.o LzmaDec.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
) else (
    gcc -o "%OUT_X86%\%~2" stub_main.o crypto.o Utils.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi
)

del /f /q *.o
endlocal
exit /b