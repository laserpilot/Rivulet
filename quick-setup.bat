@echo off
echo ============================================
echo           Rivulet Quick Setup
echo ============================================

echo.
echo Step 1: Checking CEF_ROOT environment variable...
if "%CEF_ROOT%"=="" (
    echo ❌ CEF_ROOT is not set!
    echo Please download CEF from: https://cef-builds.spotifycdn.com/index.html
    echo Then set CEF_ROOT to the extracted path:
    echo   setx CEF_ROOT "C:\path\to\your\cef_binary_xxx" /M
    pause
    exit /b 1
) else (
    echo ✅ CEF_ROOT is set to: %CEF_ROOT%
)

echo.
echo Step 2: Checking if CEF libraries are built...
if not exist "%CEF_ROOT%\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" (
    echo ❌ CEF libraries not built! Building now...
    cd "%CEF_ROOT%"
    if not exist build mkdir build
    cd build
    cmake -G "Visual Studio 17 2022" -A x64 ..
    cmake --build . --config Release
    if errorlevel 1 (
        echo ❌ CEF build failed!
        pause
        exit /b 1
    )
    echo ✅ CEF libraries built successfully!
) else (
    echo ✅ CEF libraries are ready
)

echo.
echo Step 3: Building Rivulet...
cd /d "%~dp0"
if not exist build mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release

if errorlevel 1 (
    echo ❌ Rivulet build failed!
    pause
    exit /b 1
)

echo.
echo ✅ Build complete! 
echo.
echo To run Rivulet:
echo   cd bin\Release
echo   .\Rivulet.exe
echo.
echo To test Spout output, download SpoutReceiver from:
echo   https://spout.zeal.co/download/
echo.
pause