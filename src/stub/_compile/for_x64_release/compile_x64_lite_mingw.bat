@echo off
set "CC_PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "PATH=%CC_PATH%;%PATH%"

set LZMADEC_PATH=F:\_lzy_files\lh_codes\XBat\src\common\lzma_sdk\lzmadec
set STUB_ROOT_PATH=F:\_lzy_files\lh_codes\XBat\src\stub
set STUB_FULL_PATH=%STUB_ROOT_PATH%\stub_full
set XBAT_COMMON_PATH=F:\_lzy_files\lh_codes\XBat\src\common
set STUB_X64_OUTPUT_PATH=F:\_lzy_files\lh_codes\XBat\bin\x64\templates\x64

if not exist "%STUB_X64_OUTPUT_PATH%" mkdir "%STUB_X64_OUTPUT_PATH%"

:: --- 编译选项 ---
:: 使用 -Os 优化体积，同时保留必要的对齐
set "CFLAGS=-Os -DUNICODE -D_UNICODE -DBUILDING_LITE"

echo [*] Compiling modules...

gcc %CFLAGS% -c "%STUB_ROOT_PATH%\main.c" -o main.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\crypto.c" -o crypto.o
gcc %CFLAGS% -c "%XBAT_COMMON_PATH%\Utils.c" -o Utils.o

:: --- 链接选项 ---
:: -mwindows: 指定子系统为 Windows GUI
:: -Wl,--gc-sections: 配合编译选项，剔除未使用的代码块（大幅减小体积）
:: -s: 剥离调试符号
:: -static: 静态链接基础库
set "LDFLAGS=-mwindows -Wl,--gc-sections -static -s"

echo [*] Linking executable...
gcc -o "%STUB_X64_OUTPUT_PATH%\stub_lite.bin" ^
    main.o crypto.o Utils.o ^
    %LDFLAGS% ^
    -lkernel32 -lshell32 -luser32

:: --- 清理 ---
echo [*] Cleaning temporary files...
del /f /q *.o

echo [!] Done! Output saved to: %STUB_X64_OUTPUT_PATH%\stub_lite.bin