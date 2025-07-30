@echo off
echo ============================================
echo           Debugging CEF Paths
echo ============================================

set CEF_ROOT=C:\Users\laser\Documents\GitHub\cef_binary_138.0.33+g276ed6d+chromium-138.0.7204.169_windows64

echo CEF_ROOT: %CEF_ROOT%
echo.

echo Checking CEF directory structure...
echo.
echo Contents of CEF_ROOT:
dir "%CEF_ROOT%" /b
echo.

echo Contents of include directory:
if exist "%CEF_ROOT%\include" (
    echo ✅ include directory exists
    dir "%CEF_ROOT%\include" /b | findstr /i cef_client
) else (
    echo ❌ include directory not found
)
echo.

echo Contents of build\libcef_dll_wrapper:
if exist "%CEF_ROOT%\build\libcef_dll_wrapper\Release" (
    echo ✅ CEF libraries found
    dir "%CEF_ROOT%\build\libcef_dll_wrapper\Release" /b
) else (
    echo ❌ CEF libraries not found
)

pause