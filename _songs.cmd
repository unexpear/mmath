@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake --build build_ninja_rel > build_log.txt 2>&1
echo build exit=%ERRORLEVEL%
build_ninja_rel\mmathen_tests.exe "[.songs]" > _songs.txt 2>&1
echo songs exit=%ERRORLEVEL%
build_ninja_rel\mmathen_tests.exe "wind excerpts are audition passages","brass graded notes ring when the breath stops" > _phrase.txt 2>&1
echo phrase exit=%ERRORLEVEL%
