@echo off
set "CC_PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "PATH=%CC_PATH%;%PATH%"

"%CC_PATH%\gcc.exe" -c LzmaDec.c -o LzmaDec.o -Os -fdata-sections -ffunction-sections -D_7ZIP_PPMD_SUPPPORT=OFF -D_7Z_NO_METHODS_DECODING_STUB