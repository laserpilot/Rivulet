@echo off
echo Checking CEF files...
echo.

echo Changing to script directory...
cd /d "%~dp0"

echo Current directory:
cd

echo.
echo Contents of cef directory:
if exist cef (
    echo ✅ cef directory exists
    dir cef /b | head -10
) else (
    echo ❌ cef directory not found
)

echo.
echo Contents of cef\build:
if exist cef\build (
    echo ✅ cef\build exists
    dir cef\build /b | head -10
) else (
    echo ❌ cef\build not found
)

echo.
echo Contents of cef\build\libcef_dll_wrapper:
if exist cef\build\libcef_dll_wrapper (
    echo ✅ cef\build\libcef_dll_wrapper exists
    dir cef\build\libcef_dll_wrapper /b
) else (
    echo ❌ cef\build\libcef_dll_wrapper not found
)

echo.
echo Contents of cef\build\libcef_dll_wrapper\Release:
if exist cef\build\libcef_dll_wrapper\Release (
    echo ✅ cef\build\libcef_dll_wrapper\Release exists
    dir cef\build\libcef_dll_wrapper\Release /b
) else (
    echo ❌ cef\build\libcef_dll_wrapper\Release not found
)

echo.
echo Checking specific file:
if exist "cef\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib" (
    echo ✅ libcef_dll_wrapper.lib found!
    dir "cef\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib"
) else (
    echo ❌ libcef_dll_wrapper.lib not found
)

pause