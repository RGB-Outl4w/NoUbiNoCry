@echo off
rem Builds dinput8.dll (the mod) and selftest.exe with x86 MSVC.
rem   build.cmd           build only
rem   build.cmd package   build, then zip the release into dist\NoUbiNoCry-v<version>.zip
setlocal
cd /d "%~dp0"

for /f "tokens=3" %%v in ('findstr /b /c:"#define NUNC_VERSION_STR" version.h') do set "VERSION=%%~v"
if not defined VERSION (echo Could not read the version from version.h & exit /b 1)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS=%%i"
if not defined VS (echo Visual Studio with C++ tools not found & exit /b 1)
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1 || exit /b 1

echo Building NoUbiNoCry %VERSION%
rc /nologo /fo noubinocry.res noubinocry.rc || exit /b 1
cl /nologo /O1 /MT /W3 /LD noubinocry.c noubinocry.res /link /OUT:dinput8.dll || exit /b 1
cl /nologo /O1 /MT /W3 /DSELFTEST noubinocry.c /link /OUT:selftest.exe || exit /b 1
del /q *.obj *.exp *.lib *.res 2>nul

if /i not "%~1"=="package" exit /b 0
if exist dist rmdir /s /q dist
mkdir dist
powershell -NoProfile -Command "Compress-Archive -Path dinput8.dll,README.md,LICENSE -DestinationPath 'dist\NoUbiNoCry-v%VERSION%.zip'" || exit /b 1
echo Packaged dist\NoUbiNoCry-v%VERSION%.zip
