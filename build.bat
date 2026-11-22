@echo off
rem Builds build\Epingle.exe with MSVC and the Windows SDK.
rem Run from a "Developer Command Prompt for VS" (or "x64 Native Tools Command Prompt").
setlocal
cd /d "%~dp0"
if not exist build mkdir build

pushd src
rc /nologo /fo ..\build\app.res app.rc || (popd & exit /b 1)
popd

rem /MT        : C runtime linked statically, no DLL to install
rem /GS /sdl   : stack and security checks       /guard:cf : Control Flow Guard
rem /DEPENDENTLOADFLAG:0x800 : system DLLs are only ever loaded from System32
cl /nologo /std:c17 /O2 /W4 /MT /GS /sdl /guard:cf /utf-8 /Fobuild\ /Febuild\Epingle.exe src\main.c build\app.res ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /GUARD:CF /CETCOMPAT ^
   /DEPENDENTLOADFLAG:0x800 /OPT:REF /OPT:ICF ^
   user32.lib gdi32.lib shell32.lib advapi32.lib dwmapi.lib shcore.lib || exit /b 1

echo Built build\Epingle.exe
