@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%

set "ROOT_DIR=%~dp0.."
cmake -S "%ROOT_DIR%" -B "%ROOT_DIR%\build-win-native" -G Ninja ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_TOOLCHAIN_FILE="%ROOT_DIR%\external\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DFUSE_LIBSPECTRUM_ROOT="%ROOT_DIR%\external\vcpkg\installed\x64-windows"
exit /b %errorlevel%