@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%

set "ROOT_DIR=%~dp0.."
set "PORTS_DIR=%~dp0..\vcpkg-ports"

"%ROOT_DIR%\external\vcpkg\vcpkg.exe" remove --classic --triplet x64-windows libspectrum
if errorlevel 1 exit /b %errorlevel%

"%ROOT_DIR%\external\vcpkg\vcpkg.exe" install --classic --triplet x64-windows --overlay-ports="%PORTS_DIR%" libspectrum
exit /b %errorlevel%