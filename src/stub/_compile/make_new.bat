@echo off
setlocal

:: ============================================================
:: 1. 公共路径配置
:: ============================================================
set "XBAT_ROOT=F:\_lzy_files\lh_codes\XBat"
set "XBAT_COMMON_PATH=%XBAT_ROOT%\src\common"
set "STUB_ROOT_PATH=%XBAT_ROOT%\src\stub"

:: 编译器路径
set "MINGW64_BIN=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "MINGW32_BIN=C:\Program Files\RedPanda-Cpp\mingw32\bin"

:: 输出目录
set "OUT_X64=%XBAT_ROOT%\bin\x64\templates\x64"
set "OUT_X86=%XBAT_ROOT%\bin\x64\templates\x86"

:: 自定义入口点
set "ENTRY_NAME=MyMain"

:: ============================================================
:: 2. 执行编译流程
:: ============================================================

echo ============================================================
echo 正在启动全 No-CRT 双架构编译任务 (精简合并版)...
echo ============================================================

:: --- 任务 1: x64 窗口版模板 (代替原 Full GUI / Lite) ---
call :compile_mingw_x64 "GUI" "stub_gui.bin" "" "windows"

:: --- 任务 2: x64 控制台版模板 (代替原 Full CLI) ---
call :compile_mingw_x64 "CLI" "stub_cli.bin" "-DMODE_CLI" "console"


:: --- 任务 3: x86 窗口版模板 (代替原 Full GUI / Lite) ---
call :compile_mingw_x86 "GUI" "stub_gui.bin" "" "windows"

:: --- 任务 4: x86 控制台版模板 (代替原 Full CLI) ---
call :compile_mingw_x86 "CLI" "stub_cli.bin" "-DMODE_CLI" "console"


echo.
echo ============================================================
echo 所有构建任务已成功结束 (纯净零 CRT 污染)！
echo ============================================================
pause
exit /b

:: ============================================================
:: 函数: MinGW 编译链接逻辑 (x64)
:: ============================================================
:compile_mingw_x64
echo [*] 正在编译 x64 %~1 模板...
setlocal
set "PATH=%MINGW64_BIN%;%PATH%"
if not exist "%OUT_X64%" mkdir "%OUT_X64%"

:: 编译优化参数 (保持 -ffreestanding 及禁用内建优化)
set "CFLAGS=-Os -pipe -ffreestanding -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-memcmp -mno-stack-arg-probe -fno-ms-extensions -ffunction-sections -fdata-sections -fno-ident -fno-asynchronous-unwind-tables -DUNICODE -D_UNICODE -DBUILDING %~3"
set "LDFLAGS=-m%~4 -nostdlib -e %ENTRY_NAME% -Wl,--gc-sections -s"

:: 编译公共核心组件
gcc %CFLAGS% -c "%STUB_ROOT_PATH%\stub_main.c" -o stub_main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

:: 链接最终生成无 CRT 二进制（移除了 Lzma 和 stub_full 依赖）
gcc -o "%OUT_X64%\%~2" stub_main.o crypto.o Utils.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi

del /f /q *.o
endlocal
exit /b

:: ============================================================
:: 函数: MinGW 编译链接逻辑 (x86)
:: ============================================================
:compile_mingw_x86
echo [*] 正在编译 x86 %~1 模板...
setlocal
set "PATH=%MINGW32_BIN%;%PATH%"
if not exist "%OUT_X86%" mkdir "%OUT_X86%"

set "CFLAGS=-m32 -Os -pipe -ffreestanding -fno-builtin-memcpy -fno-builtin-memset -fno-builtin-memcmp -mno-stack-arg-probe -fno-ms-extensions -ffunction-sections -fdata-sections -fno-ident -fno-asynchronous-unwind-tables -DUNICODE -D_UNICODE -DBUILDING %~3"
:: x86 下使用相应的 stdcall 导出符号格式
set "LDFLAGS=-m%~4 -nostdlib -e _%ENTRY_NAME%@0 -Wl,--gc-sections -s"

:: 编译公共核心组件
gcc %CFLAGS% -c "%STUB_ROOT_PATH%\stub_main.c" -o stub_main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

:: 链接最终生成无 CRT 二进制（移除了 Lzma 和 stub_full 依赖）
gcc -o "%OUT_X86%\%~2" stub_main.o crypto.o Utils.o %LDFLAGS% -lkernel32 -lshell32 -luser32 -lshlwapi

del /f /q *.o
endlocal
exit /b