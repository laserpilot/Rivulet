# CEF Setup Guide

This project requires the CEF (Chromium Embedded Framework) binary distribution to build. The CEF files are not included in the repository due to their large size (240MB+).

## Required CEF Version

- **CEF Version**: 131.2.7+ge7974c7+chromium-131.0.6778.85
- **Platform**: Windows 64-bit
- **Architecture**: x64

## Setup Instructions

### 1. Download CEF Binary Distribution

Download the CEF binary distribution from the official CEF website:

```
https://cef-builds.spotifycdn.com/cef_binary_131.2.7%2Bge7974c7%2Bchromium-131.0.6778.85_windows64.tar.bz2
```

Or browse available builds at: https://cef-builds.spotifycdn.com/index.html

### 2. Extract CEF

1. Extract the downloaded archive to the project root directory
2. Rename the extracted folder to `cef-binary`

The directory structure should look like:
```
Rivulet/
├── cef-binary/           # CEF binary distribution (NOT in git)
│   ├── CMakeLists.txt
│   ├── include/
│   ├── libcef_dll/
│   ├── Release/
│   ├── Resources/
│   └── cmake/
├── src/
├── lib/
└── CMakeLists.txt
```

### 3. Verify Setup

After extracting CEF, you should be able to build the project:

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Alternative: Download Script

You can also use 7-Zip to extract directly (if you have it installed):

```bash
# Download CEF (you'll need to do this manually or use curl/wget)
# Extract using 7-Zip
"C:\Program Files\7-Zip\7z.exe" x cef_binary_*.tar.bz2
"C:\Program Files\7-Zip\7z.exe" x cef_binary_*.tar
# Rename the extracted folder to cef-binary
```

## Important Notes

- CEF files are in `.gitignore` and should NOT be committed to the repository
- The build system expects the CEF directory to be named exactly `cef-binary`
- Make sure you download the correct Windows 64-bit version for compatibility
- The CEF binary distribution includes all necessary headers, libraries, and resources

## Troubleshooting

**Build Error: "CEF_ROOT does not exist"**
- Make sure the `cef-binary` directory exists in the project root
- Verify the directory structure matches the expected layout

**Runtime Errors: Missing DLL files**
- The build system automatically copies CEF DLLs to the output directory
- Make sure all CEF resources are properly extracted

**Linking Errors**
- Ensure you're using the correct CEF version for your platform
- Verify that Visual Studio 2022 is properly installed

## CEF Resources

- CEF Project: https://bitbucket.org/chromiumembedded/cef
- CEF Builds: https://cef-builds.spotifycdn.com/
- CEF Documentation: https://bitbucket.org/chromiumembedded/cef/wiki/Home