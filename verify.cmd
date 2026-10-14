@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 90
where cmake & where ninja & cmake --version | findstr version
cl 2>&1 | findstr /C:"Version"
cmake -S . -B build_ninja_rel -G Ninja -DCMAKE_BUILD_TYPE=Release > configure_log.txt 2>&1
echo configure exit=%ERRORLEVEL%
cmake --build build_ninja_rel > build_log.txt 2>&1
echo build exit=%ERRORLEVEL%
build_ninja_rel\mmathen_tests.exe > test_log.txt 2>&1
echo test exit=%ERRORLEVEL%
