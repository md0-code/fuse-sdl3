@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
if errorlevel 1 exit /b %errorlevel%

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build-win-native"
set "TRIPLET=x64-windows"
set "BUILD_TYPE="
set "BUILD_TARGET="
set "CPACK_GENERATOR_ARG="
set "PORTABLE_PACKAGE_ARG="

if not "%~1"=="" set "BUILD_DIR=%~1"
if not "%~2"=="" set "TRIPLET=%~2"
if not "%~3"=="" set "BUILD_TYPE=%~3"
if not "%~4"=="" set "BUILD_TARGET=%~4"
if not "%~5"=="" set "CPACK_GENERATOR_ARG=-DCPACK_GENERATOR=%~5"
if not "%~6"=="" set "PORTABLE_PACKAGE_ARG=-DFUSE_PORTABLE_PACKAGE=%~6"

set "EXTRA_CMAKE_ARGS="
if not "%BUILD_TYPE%"=="" set "EXTRA_CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%"
if not "%CPACK_GENERATOR_ARG%"=="" set "EXTRA_CMAKE_ARGS=%EXTRA_CMAKE_ARGS% %CPACK_GENERATOR_ARG%"
if not "%PORTABLE_PACKAGE_ARG%"=="" set "EXTRA_CMAKE_ARGS=%EXTRA_CMAKE_ARGS% %PORTABLE_PACKAGE_ARG%"

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_TOOLCHAIN_FILE="%ROOT_DIR%\external\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET% ^
  -DFUSE_LIBSPECTRUM_ROOT="%ROOT_DIR%\external\vcpkg\installed\%TRIPLET%" ^
  %EXTRA_CMAKE_ARGS%
if errorlevel 1 exit /b %errorlevel%

if "%BUILD_TARGET%"=="" (
  cmake --build "%BUILD_DIR%"
) else (
  cmake --build "%BUILD_DIR%" --target "%BUILD_TARGET%"
)
exit /b %errorlevel%