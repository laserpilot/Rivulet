@echo off
echo ============================================
echo           Building Rivulet
echo ============================================

echo Changing to script directory...
cd /d "%~dp0"
echo Current directory: %CD%
echo.

echo Checking CEF libraries in local cef/ directory...
if not exist "cef\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" (
    echo ❌ CEF libraries not found in cef/ directory!
    echo.
    echo Please copy your CEF binary distribution to the cef/ directory:
    echo 1. Copy all contents from: C:\Users\laser\Documents\GitHub\cef_binary_138.0.33+g276ed6d+chromium-138.0.7204.169_windows64\*
    echo 2. Paste into: %~dp0cef\
    echo 3. Make sure cef\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib exists
    pause
    exit /b 1
)
echo ✅ CEF libraries found in local cef/ directory

echo.
echo Building Rivulet...
cd /d "%~dp0"
if not exist build mkdir build
cd build

echo.
echo Cleaning previous build configuration...
if exist CMakeCache.txt del CMakeCache.txt
if exist CMakeFiles rmdir /s /q CMakeFiles

echo.
echo Configuring Rivulet with CMake...
cmake -G "Visual Studio 17 2022" -A x64 .
if errorlevel 1 (
    echo ❌ CMake configuration failed!
    echo.
    echo Check the error messages above for details.
    pause
    exit /b 1
)

echo.
echo Building Rivulet...
cmake --build . --config Release
if errorlevel 1 (
    echo ❌ Rivulet build failed!
    echo.
    echo Check the error messages above for details.
    pause
    exit /b 1
)

echo.
echo ✅ Rivulet built successfully!
echo.
echo Executable location:
echo   %~dp0build\bin\Release\Rivulet.exe
echo.
echo To run:
echo   cd build\bin\Release
echo   .\Rivulet.exe
echo.
pause