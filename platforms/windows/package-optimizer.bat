@echo off
REM Rivulet Build Size Optimizer
REM Reduces Release folder from 1GB+ to ~300MB

echo ====================================
echo    Rivulet Package Optimizer
echo ====================================

set BUILD_DIR=build\bin\Release
set DIST_DIR=dist

echo Creating optimized distribution...
mkdir %DIST_DIR% 2>nul

REM Copy essential files
echo Copying core application...
copy "%BUILD_DIR%\Rivulet.exe" "%DIST_DIR%\"
copy "%BUILD_DIR%\libcef.dll" "%DIST_DIR%\"

REM Copy minimal CEF dependencies (only essential)
echo Copying essential CEF libraries...
copy "%BUILD_DIR%\chrome_elf.dll" "%DIST_DIR%\"
copy "%BUILD_DIR%\libEGL.dll" "%DIST_DIR%\"
copy "%BUILD_DIR%\libGLESv2.dll" "%DIST_DIR%\"

REM Copy only English locale (remove others to save ~100MB)
mkdir "%DIST_DIR%\locales" 2>nul
copy "%BUILD_DIR%\locales\en-US.pak" "%DIST_DIR%\locales\"

REM Copy essential resources
mkdir "%DIST_DIR%\swiftshader" 2>nul
copy "%BUILD_DIR%\swiftshader\*.dll" "%DIST_DIR%\swiftshader\" 2>nul

echo Copying essential resources...
copy "%BUILD_DIR%\chrome_100_percent.pak" "%DIST_DIR%\" 2>nul
copy "%BUILD_DIR%\resources.pak" "%DIST_DIR%\" 2>nul
copy "%BUILD_DIR%\icudtl.dat" "%DIST_DIR%\" 2>nul
copy "%BUILD_DIR%\v8_context_snapshot.bin" "%DIST_DIR%\" 2>nul

REM Skip optional files that bloat size:
REM - widevinecdm.dll (19MB DRM - not needed for basic browser)
REM - All locales except en-US (~100MB saved)
REM - Debug symbols and dev tools resources

echo ====================================
echo Optimization complete!
echo.
echo Size comparison:
dir "%BUILD_DIR%" | find "bytes"
echo.
echo Optimized size:
dir "%DIST_DIR%" | find "bytes"
echo.
echo Package ready in: %DIST_DIR%\
echo ====================================