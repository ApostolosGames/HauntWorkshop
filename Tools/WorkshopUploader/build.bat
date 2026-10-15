@echo off
rem Builds WorkshopUploader.exe into bin\, next to the steam_api64.dll it needs.
rem   build.bat "C:\Path\To\steamworks_sdk\sdk"
rem The SDK path can also come from the STEAMWORKS_SDK environment variable. Either the
rem official SDK layout (public\steam) or an Includes\steam layout is accepted.
setlocal
set "HERE=%~dp0"
set "SDK=%~1"
if not defined SDK set "SDK=%STEAMWORKS_SDK%"
if not defined SDK (echo Pass the Steamworks SDK folder, or set STEAMWORKS_SDK. & exit /b 1)

set "INC=%SDK%\public"
if not exist "%INC%\steam\steam_api.h" set "INC=%SDK%\Includes"
if not exist "%INC%\steam\steam_api.h" (echo No steam\steam_api.h under "%SDK%". & exit /b 1)
set "BIN=%SDK%\redistributable_bin\win64"

where cl >nul 2>nul && goto :build
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "delims=" %%i in ('call "%%VSWHERE%%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS=%%i"
if not defined VS (echo Visual Studio with C++ tools not found. & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

:build
if not exist "%HERE%bin" mkdir "%HERE%bin"
cl /nologo /std:c++17 /EHsc /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /I"%INC%" "%HERE%WorkshopUploader.cpp" ^
   /Fo"%HERE%bin\\" /Fe"%HERE%bin\WorkshopUploader.exe" ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED "%BIN%\steam_api64.lib" || exit /b 1
copy /y "%BIN%\steam_api64.dll" "%HERE%bin\" >nul
del "%HERE%bin\*.obj" 2>nul
echo Built %HERE%bin\WorkshopUploader.exe
