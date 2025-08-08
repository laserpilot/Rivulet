# Rivulet Windows Platform

Modern CEF-Spout video sharing application for Windows with hardware-accelerated rendering and interactive web browsing.

## 🚀 Quick Start (End Users)

Download the latest release and see `Rivulet-v1.0.0-Windows\README.txt` for user documentation.

## 🛠️ Development Setup

### Prerequisites
- Windows 10/11 x64
- Visual Studio 2022 with C++ development tools
- CMake 3.20+
- Git

### Build Instructions

1. **Clone and Setup:**
```bash
git clone <repository-url>
cd Rivulet/platforms/windows
```

2. **Download CEF:**
```bash
# CEF binaries are included - no separate download needed
# If updating CEF, place new binaries in cef/ directory
```

3. **Build Rivulet:**
```bash
build-rivulet.bat
```

4. **Run:**
```bash
build\bin\Release\Rivulet.exe
```

### Project Structure
```
platforms/windows/
├── src/                    # Rivulet source code
│   ├── main.cpp           # Application entry point
│   ├── application.cpp    # Main application class
│   ├── rivulet_browser_window.cpp  # Main browser window
│   └── spout_sender.cpp   # Spout2 integration
├── cef/                   # CEF binaries and headers
├── lib/                   # External libraries
│   └── Spout2/           # Spout2 SDK
├── build/                 # Build output directory
└── README.md             # This file
```

## 🎯 Recent Major Features

### Resolution Control System
- **Dynamic Resolution Changes**: Switch between presets or enter custom resolutions
- **Apply Button UX**: Select first, then apply for better user control
- **Validation**: Resolution bounds checking (100x100 to 7680x4320)
- **Aspect Ratio Preservation**: Window automatically resizes to maintain proper proportions

### Console Management
- **Quiet Operation**: Verbose runtime output now optional
- **F12 Toggle**: Show/hide console window on demand
- **Command Line Flags**: `--verbose`, `--hide-console` for different use cases
- **Smart Startup**: Show important messages, hide noise during operation

### Input System Fixes
- **Perfect Mouse Tracking**: Fixed coordinate mapping drift issues
- **Letterboxing Aware**: Mouse coordinates properly mapped to content area
- **Consistent Scaling**: Uses same math as rendering pipeline for accuracy

## 🔧 Technical Architecture

### DirectX 11 Hardware Acceleration
- **Adapter Synchronization**: Ensures CEF and DirectX use same GPU
- **Shared Texture Pipeline**: Zero-copy rendering for maximum performance
- **V-Sync Locked Rendering**: Smooth 60 FPS output with perfect frame pacing
- **BGRA Format**: Compatible with CEF's preferred color format

### Spout2 Integration
- **Hardware Accelerated**: Direct D3D11 texture sharing
- **Zero-Copy**: GPU-to-GPU transfer without CPU involvement
- **Real-time**: Sub-frame latency for live video applications
- **Compatible**: Works with all major creative applications

### CEF Browser Engine
- **Chromium Based**: Latest web standards support
- **Off-screen Rendering**: Custom rendering pipeline for video sharing
- **Hardware Accelerated**: WebGL, CSS3D, and modern web features
- **Interactive**: Full mouse and keyboard support

## 🐛 Development Notes

### Build System
- Uses CMake with custom CEF integration
- Automatic library linking and dependency management
- Release builds optimized for distribution

### Debugging
- Console output controlled by verbosity flags
- DirectX debug layer available in debug builds
- GPU adapter enumeration and selection logging

### Performance Considerations
- V-Sync locked rendering prevents screen tearing
- Frame pacing ensures consistent output timing
- GPU adapter selection favors discrete graphics
- Memory management optimized for long-running operation

## 📦 Distribution

The application can be packaged for distribution using the release scripts:
- `create-release-package.bat` - Creates distributable package
- `package-optimizer.bat` - Optimizes package size

## 🔄 Version History

### Latest Update
- ✅ Fixed mouse coordinate mapping drift
- ✅ Added resolution Apply button for better UX
- ✅ Implemented console management with F12 toggle
- ✅ Added verbose logging controls
- ✅ Improved startup and shutdown handling

### Previous Releases
- Hardware acceleration with DirectX 11
- Spout2 integration with zero-copy rendering
- CEF browser engine integration
- Professional browser controls
- Aspect ratio correction and letterboxing

## 🤝 Contributing

This is part of the larger Rivulet cross-platform project. See the main repository README for contributing guidelines.

## 📄 License

See LICENSE file in the root directory.

---

Built with ❤️ for the creative community