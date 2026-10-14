@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > _%1_build.txt 2>&1
echo build exit=%ERRORLEVEL%
if exist _wav_%1 rmdir /s /q _wav_%1
mkdir _wav_%1
set MMATH_WAV_DIR=%CD%\_wav_%1
build_ninja_rel\mmathen_tests.exe "[wind],[.songs]" > _%1_run.txt 2>&1
echo run exit=%ERRORLEVEL%
