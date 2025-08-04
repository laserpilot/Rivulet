@echo off
echo ============================================
echo           Building CEF Libraries
echo ============================================

set CEF_ROOT=%~dp0cef

echo CEF_ROOT is set to: %CEF_ROOT%
echo.

echo Checking if CEF directory exists...
if not exist "%CEF_ROOT%" (
    echo ❌ ERROR: CEF directory not found at %CEF_ROOT%
    echo CEF should be located at: platforms/windows/cef/
    echo Please verify the CEF framework is in place.
    pause
    exit /b 1
)
echo ✅ CEF directory found

echo.
echo Creating CEF build directory...
cd "%CEF_ROOT%"
if not exist build mkdir build
cd build

echo.
echo Configuring CEF with CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..
if errorlevel 1 (
    echo ❌ CMake configuration failed!
    echo.
    echo Common issues:
    echo 1. Visual Studio 2022 not installed
    echo 2. CMake not in PATH
    echo 3. Missing C++ build tools
    echo.
    pause
    exit /b 1
)

echo.
echo Building CEF libraries (this may take 5-10 minutes)...
cmake --build . --config Release
if errorlevel 1 (
    echo ❌ CEF build failed!
    pause
    exit /b 1
)

echo.
echo ✅ CEF libraries built successfully!
echo.
echo Key files created:
dir /b libcef_dll_wrapper\Release\libcef_dll_wrapper.lib 2>nul && echo   ✅ libcef_dll_wrapper.lib
dir /b cefsimple\Release\cefsimple.exe 2>nul && echo   ✅ cefsimple.exe (test app)

echo.
echo CEF is now ready for Rivulet build!
pause