@echo off
set "CC_PATH=C:\Program Files\RedPanda-Cpp\mingw64\bin"
set "PATH=%CC_PATH%;%PATH%"

"%CC_PATH%\gcc" -c LzmaEnc.c -o LzmaEnc.o -O2
"%CC_PATH%\gcc" -c LzFind.c -o LzFind.o -O2
"%CC_PATH%\gcc" -c LzFindMt.c -o LzFindMt.o -O2
"%CC_PATH%\gcc" -c LzFindOpt.c -o LzFindOpt.o -O2
"%CC_PATH%\gcc" -c Threads.c -o Threads.o -O2
"%CC_PATH%\gcc" -c CpuArch.c -o CpuArch.o -O2

pause