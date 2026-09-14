@echo off
rem Configure + build the full CASLStudio app (core + tests + Qt UI).
rem Proxy env vars are cleared: duplicate NO_PROXY/no_proxy crash MSBuild.
set http_proxy=
set HTTP_PROXY=
set https_proxy=
set HTTPS_PROXY=
set no_proxy=
set NO_PROXY=
set all_proxy=
set ALL_PROXY=

set QTDIR=C:\Qt\6.8.3\msvc2022_64
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat

call "%VCVARS%" || exit /b 1
cd /d "%~dp0"
cmake -S . -B build -DCMAKE_PREFIX_PATH=%QTDIR% || exit /b 1
cmake --build build --config Release || exit /b 1
echo BUILD_OK
