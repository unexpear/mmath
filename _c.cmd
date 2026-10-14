@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _c_build.txt 2>&1
echo build exit=%ERRORLEVEL%
build_ninja_rel\mmathen_tests.exe "[contact]" > _c_run.txt 2>&1
echo run exit=%ERRORLEVEL%
