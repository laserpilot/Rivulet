# Rivulet - Cross-Platform Video Sharing

**Organized project structure for Windows (CEF + Spout) and macOS (Rust + Syphon) video sharing implementations**

This repository contains separate, independent implementations for real-time video sharing on Windows and macOS platforms.

## Project Structure

```
Rivulet/
├── platforms/
│   ├── windows/          # Windows Implementation
│   │   ├── cef/          # Chromium Embedded Framework
│   │   ├── lib/          # Spout2 SDK and libraries
│   │   └── src/          # HTML/JS application interface
│   └── macos/            # macOS Implementation (to be merged)
│       └── src-tauri/    # Rust + Tauri + Syphon integration
├── docs/                 # Technical documentation
└── shared/               # Cross-platform resources
```

## Platform Implementations

### Windows (CEF + Spout)
- **Technology**: Chromium Embedded Framework + Spout2
- **Graphics**: DirectX 11 texture sharing
- **Language**: C++ with HTML/JS interface
- **Status**: ✅ Ready for development

### macOS (Rust + Syphon)
- **Technology**: Tauri + Rust + Syphon Framework
- **Graphics**: OpenGL/Metal IOSurface sharing
- **Language**: Rust with Tauri app framework
- **Status**: 🚧 To be merged from separate branch

## Development Setup

### Windows Development

1. **Prerequisites**:
   - Windows 10/11
   - Visual Studio 2022 with C++ support
   - CMake

2. **Build CEF Application**:
   ```bash
   cd platforms/windows/cef/build
   # Open cef.sln in Visual Studio or use:
   cmake --build . --config Release
   ```

3. **Run Example**:
   ```bash
   cd platforms/windows/cef/build/tests/cefsimple/Release
   ./cefsimple.exe
   ```

### macOS Development (Future)

When the macOS branch is merged:

```bash
cd platforms/macos/src-tauri
cargo run
```

## Project Architecture

### Separate Platform Implementations

This project maintains **separate, independent implementations** for each platform rather than attempting to unify them:

**Why Separate?**
- **Different Technologies**: Windows uses CEF+C++, macOS uses Rust+Tauri
- **Platform-Specific Optimization**: Each implementation leverages platform strengths
- **Independent Development**: Teams can work on each platform without conflicts
- **Easier Maintenance**: Platform-specific bugs don't affect other platforms

### Current Status

| Platform | Implementation | Build System | Status |
|----------|---------------|--------------|---------|
| **Windows** | CEF + Spout2 | Visual Studio + CMake | ✅ Ready |
| **macOS** | Rust + Syphon | Cargo + Tauri | 🚧 Separate branch |

## Platform-Specific Information

### Windows (CEF + Spout)

**Files**: `platforms/windows/`

- **CEF Framework**: Complete browser integration
- **Spout2 SDK**: DirectX texture sharing
- **HTML Interface**: Browser-based UI in `src/`
- **Build**: Visual Studio solution ready to compile

**Key Components**:
- `platforms/windows/cef/` - CEF framework and examples
- `platforms/windows/lib/Spout2/` - Spout2 SDK
- `platforms/windows/src/` - Application HTML/JS interface

### macOS (Rust + Syphon)

**Files**: `platforms/macos/` (to be merged)

- **Tauri Framework**: Rust-based app framework
- **Syphon Integration**: OpenGL/Metal video sharing
- **Native Performance**: Zero-copy IOSurface streaming
- **Build**: Cargo-based Rust toolchain

## Documentation

### Technical Documentation
- **[Architecture Requirements](docs/ARCHITECTURE-REQUIREMENTS.md)** - Technical architecture overview
- **[Build Instructions](docs/BUILD-AND-TEST-INSTRUCTIONS.md)** - Detailed build setup
- **[Current Status](docs/current-summary.md)** - Implementation progress

### Platform-Specific Guides
- **[Windows Spout Integration](docs/SPOUT2-SDK-INTEGRATION.md)** - Windows-specific setup
- **[macOS Tauri Implementation](docs/TAURI-IMPLEMENTATION.md)** - macOS-specific setup

## Getting Started

### Clone Repository
```bash
git clone https://github.com/yourusername/rivulet.git
cd rivulet
```

### Windows Development
```bash
# Open the CEF solution in Visual Studio
cd platforms/windows/cef/build
start cef.sln

# Or build from command line
cmake --build . --config Release
```

### macOS Development (Future)
```bash
# When macOS branch is merged
cd platforms/macos/src-tauri
cargo run
```

## Compatible Applications

### Windows (Spout2)
- [OBS Studio](https://obsproject.com/) (with Spout2 plugin)
- [Resolume Arena/Avenue](https://resolume.com/)
- [TouchDesigner](https://derivative.ca/)
- [MadMapper](https://madmapper.com/)
- [Spout Receiver](https://github.com/leadedge/Spout2/releases) (testing tool)

### macOS (Syphon)
- [OBS Studio](https://obsproject.com/) (with Syphon plugin)
- [Resolume Arena/Avenue](https://resolume.com/)
- [VDMX](https://vidvox.net/)
- [Millumin](https://www.millumin.com/)
- [Syphon Recorder](http://syphon.info/) (testing tool)

## Contributing

### Development Workflow
1. **Platform Focus**: Choose Windows or macOS for development
2. **Feature Branch**: Create feature branches for specific platforms
3. **Platform Testing**: Test thoroughly on target platform
4. **Documentation**: Update platform-specific documentation

### Code Organization
- Windows development in `platforms/windows/`
- macOS development in `platforms/macos/` (when merged)
- Shared documentation in `docs/`
- Cross-platform resources in `shared/`

## License

This project builds upon multiple open-source technologies:
- **Spout2**: MIT License (Windows video sharing)
- **Syphon**: MIT License (macOS video sharing)
- **CEF**: BSD License (Chromium Embedded Framework)
- **Tauri**: MIT/Apache License (Rust app framework)

## Related Projects

### Video Sharing Frameworks
- [Spout2](https://github.com/leadedge/Spout2) - Windows real-time video sharing
- [Syphon](https://github.com/Syphon/Syphon-Framework) - macOS real-time video sharing

### Application Frameworks
- [CEF](https://github.com/chromiumembedded/cef) - Chromium Embedded Framework
- [Tauri](https://tauri.app/) - Rust-based app development

---

**Transform your applications into professional video sources with Rivulet's platform-specific video sharing implementations.**