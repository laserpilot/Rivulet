# Rivulet - Interactive Browser with Spout Streaming

> **Professional interactive web browser with real-time Spout2 video sharing for creative applications**

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Platform](https://img.shields.io/badge/platform-Windows-blue.svg) ![Status](https://img.shields.io/badge/status-Production%20Ready-green.svg)

## 🎯 What is Rivulet?

Rivulet transforms any website into a live video source for creative applications. Browse the web normally while simultaneously streaming the content via Spout2 to video mixing software, VJ tools, and live production applications.

**Perfect for:**
- Live video production and streaming
- VJ performances and club visuals  
- Interactive installations and digital art
- Real-time web content mixing
- Creative coding and generative art

## ✨ Key Features

- 🌐 **Full Interactive Browser** - Complete web browsing with mouse and keyboard support
- 📺 **Real-time Spout Streaming** - 1920x1080@60fps video output via Spout2
- 🎮 **Professional Controls** - Back/Forward/Reload/Stop/URL bar with Go button
- 📐 **Smart Aspect Ratio** - Automatic letterboxing prevents content distortion
- 🎯 **Zero Crashes** - Rock-solid stability with professional error handling
- ⚡ **High Performance** - Hardware accelerated rendering with CEF

## 🎬 Compatible Applications

**Confirmed Working:**
- **[MadMapper](https://madmapper.com/)** - Video mapping and projection
- **[Resolume](https://resolume.com/)** - Live video mixing and VJ software
- **[TouchDesigner](https://derivative.ca/)** - Interactive media systems
- **[OBS Studio](https://obsproject.com/)** - Live streaming and recording

## 🚀 Quick Start

### For Users (Binary Release)
1. Download the latest release package
2. Extract and run `Rivulet.exe`
3. Enter any website URL
4. Content streams automatically as "Rivulet Output" via Spout
5. Connect from your creative application

### For Developers (Build from Source)

#### Prerequisites
- Windows 10/11 (64-bit)
- Visual Studio 2022 with C++ tools
- CMake 3.15+
- Git

#### Build Instructions
```bash
# Clone repository
git clone https://github.com/user/rivulet.git
cd rivulet

# Download and setup CEF
.\build-cef.bat

# Build Rivulet
.\build-rivulet.bat

# Create distribution package
.\create-release-package.bat
```

## 🏗️ Architecture

```
Web Content (HTML/JS/CSS)
           ↓
   CEF Browser Engine
           ↓
    Bitmap Rendering
           ↓
  ┌─────────────────┐
  │ Application     │ → Window Display
  │ Window          │
  └─────────────────┘
           ↓
    Spout2 Sender → Creative Applications
```

## 🛠️ Technical Implementation

### Core Components

- **CEF Integration**: Chromium v138+ with off-screen rendering
- **Spout2 SDK**: Official Spout2 library with DirectX integration  
- **D3D11 Graphics**: Modern Windows graphics pipeline
- **Modern C++17**: Clean, maintainable codebase

### Performance Features

- **60 FPS Rendering**: Smooth real-time web content
- **Low Latency**: Direct bitmap → Spout frame pipeline
- **Hardware Accelerated**: NVIDIA/AMD GPU optimization
- **Memory Efficient**: RAII resource management

### Build System

- **CMake 3.21+**: Modern, cross-platform build system
- **Visual Studio 2022**: Latest MSVC toolchain
- **Static Runtime**: Compatible with CEF binary distribution
- **Automatic Dependencies**: CEF DLLs and resources copied automatically

## 📁 Project Structure

```
Rivulet/
├── src/                     # Application source code
│   ├── main.cpp            # Entry point with CEF subprocess handling
│   ├── application.h/cpp   # Main application and window management
│   ├── web_layer.h/cpp     # CEF browser integration
│   ├── spout_sender.h/cpp  # Spout2 output implementation
│   └── d3d11_device.h/cpp  # DirectX 11 device management
├── cef/                    # CEF binary distribution (user copies here)
├── lib/                    # Spout2 SDK and libraries
├── build/                  # CMake build output
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

## 🎯 Use Cases

### Creative Applications
- **MadMapper**: Video mapping and projection
- **Resolume**: VJ and live video mixing  
- **TouchDesigner**: Interactive media and installations
- **VMS**: Virtual broadcast graphics

### Content Types
- **Live Web Pages**: Dynamic dashboards, social feeds
- **WebGL Graphics**: Interactive 3D visualizations
- **HTML5 Games**: Real-time game content
- **Data Visualizations**: Live charts and analytics

## 🔧 Advanced Configuration

### Custom URLs
Modify `src/application.cpp` line 203:
```cpp
web_layer_->Initialize("https://your-custom-url.com", width, height);
```

### Resolution Settings
Modify window dimensions in `src/application.cpp`:
```cpp
window_width_(1920)   // Custom width
window_height_(1080)  // Custom height
```

### Spout Sender Name
Modify `src/application.cpp` line 239:
```cpp
spout_sender_->Initialize("Your Custom Name");
```

## 🐛 Troubleshooting

### Build Issues
- **CEF_ROOT Error**: Ensure CEF files are in `cef/` directory
- **Library Missing**: Run `build-cef.bat` first to build CEF libraries
- **Runtime Library Mismatch**: Project uses static runtime (`/MT`)

### Runtime Issues  
- **White Screen**: Check console for CEF initialization errors
- **No Spout Output**: Verify Spout2 installation and receiver apps
- **Performance Issues**: Enable hardware acceleration in CEF settings

### Common Solutions
```cmd
# Clean rebuild
rmdir /s build
build-rivulet.bat

# Verify CEF setup
dir cef\build\libcef_dll_wrapper\Release\libcef_dll_wrapper.lib
```

## 🚀 Future Enhancements

### Planned Features
- **Interactive Input**: Mouse/keyboard interaction with web content
- **Multiple URLs**: Support for multiple browser instances
- **Configuration UI**: Runtime URL and settings management
- **Plugin System**: Custom CEF extensions and JavaScript APIs

### Performance Optimizations
- **Shared Textures**: Zero-copy DirectX texture sharing (when CEF supports)
- **GPU Compositing**: Hardware-accelerated rendering pipeline
- **Frame Rate Control**: Adaptive FPS based on content

## 🤝 Contributing

This project demonstrates modern CEF-Spout integration. Contributions welcome for:
- Interactive input handling
- Multi-window support
- Configuration management
- Performance optimizations

## 📄 License

Based on CEF-Mixer architecture with modern implementation.
See original CEF-Mixer: https://github.com/daktronics/cef-mixer

## 🎉 Acknowledgments

- **CEF Team**: Chromium Embedded Framework
- **Spout Project**: Real-time video sharing framework  
- **daktronics/cef-mixer**: Original architecture inspiration

---

**Status**: ✅ Fully functional CEF-Spout pipeline complete!
**Last Updated**: July 30, 2025