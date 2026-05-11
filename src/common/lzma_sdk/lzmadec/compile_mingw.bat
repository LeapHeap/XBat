@echo off
set "CC_PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "PATH=%CC_PATH%;%PATH%"

set LZMADEC_PATH=F:\_lzy_files\lh_codes\XBat\src\common\lzma_sdk\lzmadec
set STUB_ROOT_PATH=F:\_lzy_files\lh_codes\XBat\src\stub
set STUB_FULL_PATH=%STUB_ROOT_PATH%\stub_full
set XBAT_COMMON_PATH=F:\_lzy_files\lh_codes\XBat\src\common
set STUB_X64_OUTPUT_PATH=F:\_lzy_files\lh_codes\XBat\bin\x64\templates\x64

"%CC_PATH%\gcc.exe" -c LzmaDec.c -o LzmaDec.o -Os -fdata-sections -ffunction-sections -D_7ZIP_PPMD_SUPPPORT=OFF -D_7Z_NO_METHODS_DECODING_STUB