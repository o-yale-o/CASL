@echo off
rem Build QScintilla 2.14.1 against Qt 6.8.3 (msvc2022_64).
rem Proxy env vars are cleared: duplicate NO_PROXY/no_proxy crash MSBuild.
set http_proxy=
set HTTP_PROXY=
set https_proxy=
set HTTPS_PROXY=
set no_proxy=
set NO_PROXY=
set all_proxy=
set ALL_PROXY=

set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat
set QMAKE=C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe

call "%VCVARS%" || exit /b 1
cd /d "%~dp0third_party\QScintilla_src-2.14.1\src" || exit /b 1
"%QMAKE%" qscintilla.pro || exit /b 1
nmake /NOLOGO release || exit /b 1
echo QSCINTILLA_BUILD_OK
