@echo off
call compile_x64_full_mingw
call compile_x64_lite_mingw
call compile_x86_full_vc98
call compile_x86_lite_vc98

echo.
echo ========================================
echo All compilations are finished!
echo ========================================
pause