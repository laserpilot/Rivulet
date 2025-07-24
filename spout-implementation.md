# Windows Spout Integration Plan

## Overview
This document outlines the plan to add Windows Spout2 support to the existing Tauri-based video sharing application without affecting the current macOS Syphon implementation.

## Current Architecture Analysis

### ✅ Existing Syphon Implementation (macOS)
- **Main Entry**: `src-tauri/src/main.rs` with conditional compilation `#[cfg(target_os = "macos")]`
- **Rust Module**: `src-tauri/src/syphon_simple.rs` - High-level Rust interface
- **C Bridge**: `src-tauri/src/syphon_bridge.m` - Objective-C bridge to Syphon.framework
- **Screen Capture**: `src-tauri/src/screencapture_iosurface.m` - ScreenCaptureKit integration
- **Framework**: Universal Syphon.framework (x86_64 + arm64)
- **Performance**: Zero-copy IOSurface sharing

### 🚧 Current Windows Stubs
The codebase already has Windows placeholders in `main.rs:19-35`:
```rust
#[cfg(target_os = "windows")]
struct SpoutOutput {
    name: String,
}
// ... placeholder methods
```

## Cross-Platform Architecture Strategy

### 🏗️ Mirror Pattern Architecture
We'll replicate the successful Syphon architecture for Windows:

```
┌─────────────────────────────────────────────────────────────┐
│                    Unified Tauri Commands                   │
│            (No changes - already cross-platform)            │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                 main.rs (Enhanced)                          │
│  ┌─────────────────────────────────────────────────────────┐│
│  │  #[cfg(target_os = "macos")]     #[cfg(target_os = "windows")] ││
│  │  use syphon_simple::SyphonOutput  use spout_simple::SpoutOutput││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│              Platform-Specific Modules                      │
│  ┌─────────────────────────┬───────────────────────────────┐│
│  │                         │                               ││
│  ▼                         ▼                               ││
│ ┌─────────────────┐       ┌─────────────────────────────┐  ││
│ │ syphon_simple.rs│       │ spout_simple.rs (NEW)       │  ││
│ │ (✅ Existing)   │       │ (🆕 To Be Created)          │  ││
│ └─────────────────┘       └─────────────────────────────┘  ││
│          │                          │                     ││
│          ▼                          ▼                     ││
│ ┌─────────────────┐       ┌─────────────────────────────┐  ││
│ │ syphon_bridge.m │       │ spout_bridge.cpp (NEW)      │  ││
│ │ (✅ Existing)   │       │ (🆕 To Be Created)          │  ││
│ └─────────────────┘       └─────────────────────────────┘  ││
│          │                          │                     ││
│          ▼                          ▼                     ││
│ ┌─────────────────┐       ┌─────────────────────────────┐  ││
│ │screencapture_   │       │screencapture_d3d11.cpp     │  ││
│ │iosurface.m      │       │(🆕 To Be Created)           │  ││
│ │(✅ Existing)    │       │                             │  ││
│ └─────────────────┘       └─────────────────────────────┘  ││
└─────────────────────────────────────────────────────────────┘
```

## Implementation Plan

### Phase 1: Core Infrastructure Setup

#### 1.1 Create Windows Spout Bridge (`src-tauri/src/spout_bridge.cpp`)
**Purpose**: C++ bridge between Rust and Spout2 SDK

**Key Functions** (mirror `syphon_bridge.m`):
```cpp
extern "C" {
    SpoutServerState* spout_server_create(const char* name, void* d3d_device);
    bool spout_server_publish_frame(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height);
    bool spout_server_publish_texture(SpoutServerState* state, void* texture_handle);
    bool spout_server_has_clients(SpoutServerState* state);
    void spout_server_stop(SpoutServerState* state);
}
```

**Dependencies**:
- Spout2 SDK integration
- D3D11 for texture creation
- DXGI for desktop duplication

#### 1.2 Create Windows Rust Module (`src-tauri/src/spout_simple.rs`)
**Purpose**: High-level Rust interface matching Syphon API

**Key Structure**:
```rust
#[cfg(target_os = "windows")]
pub struct SpoutOutput {
    server_state: *mut SpoutServerState,
    name: String,
    frame_count: u64,
}

#[cfg(target_os = "windows")]
impl SpoutOutput {
    pub fn new(name: String) -> Self
    pub fn update_frame(&self, data: &[u8], width: u32, height: u32) -> bool
    pub fn start_screen_capture(&self) -> bool
    pub fn publish_screen_capture(&self) -> bool
    pub fn stop(&self)
    // Mirror all existing Syphon methods for API consistency
}
```

#### 1.3 Enhance Build System
**Update `Cargo.toml`**:
```toml
# Add Windows-specific dependencies
[target.'cfg(target_os = "windows")'.dependencies]
winapi = { version = "0.3", features = ["d3d11", "dxgi", "dxgitype", "dxgiformat"] }

[build-dependencies]
cc = "1.0"  # For C++ compilation
```

**Create `build.rs`** for Windows C++ compilation:
```rust
#[cfg(target_os = "windows")]
fn build_spout_bridge() {
    cc::Build::new()
        .cpp(true)
        .file("src/spout_bridge.cpp")
        .include("lib/spout2")  // Spout2 SDK path
        .compile("spout_bridge");
}
```

### Phase 2: Screen Capture Integration

#### 2.1 Windows Screen Capture (`src-tauri/src/screencapture_d3d11.cpp`)
**Purpose**: Windows equivalent of `screencapture_iosurface.m`

**Key Features**:
- DXGI Desktop Duplication API for screen capture
- Direct D3D11 texture creation (zero-copy)
- Window-specific capture using Win32 API
- Application-based filtering

**Functions**:
```cpp
extern "C" {
    bool spout_server_start_screen_capture();
    bool spout_server_start_window_capture(uint32_t window_id);
    bool spout_server_start_application_capture();
    bool spout_server_publish_screen_capture(SpoutServerState* state);
    bool spout_server_has_screen_frame();
    void spout_server_stop_screen_capture();
}
```

#### 2.2 Windows-Specific Tauri Commands
**Enhance existing commands in `main.rs`**:
```rust
#[cfg(target_os = "windows")]
fn initialize_screen_capture(state: State<Arc<Mutex<VideoShareState>>>) -> Result<VideoResponse, String> {
    // Windows implementation using SpoutOutput
    let mut video_state = state.inner().lock()?;
    match video_state.initialize_screen_capture() {
        Ok(()) => Ok(VideoResponse { success: true, ... }),
        Err(e) => Err(format!("Windows screen capture failed: {}", e))
    }
}
```

### Phase 3: API Unification & Testing

#### 3.1 Unified JavaScript Interface
**No changes needed** - existing Tauri commands already work cross-platform:
```javascript
// Same API on both Windows and macOS
await invoke('initialize_video_sharing');
await invoke('initialize_screen_capture');
await invoke('publish_frame', { frame: frameData });
```

#### 3.2 Cross-Platform Testing
**Add Windows-specific test scenarios**:
- Spout server creation and client detection
- D3D11 texture sharing performance
- Desktop duplication functionality
- Window-specific capture on Windows

### Phase 4: Documentation & Examples

#### 4.1 Cross-Platform Setup Instructions
**Update README.md**:
- Windows: Visual Studio, vcpkg, Spout2 SDK setup
- macOS: Existing Xcode, Syphon.framework setup
- Build commands for both platforms

#### 4.2 Platform-Specific Examples
- Windows: Integration with OBS, TouchDesigner, MadMapper
- macOS: Existing Syphon receiver compatibility

## Technical Implementation Details

### Dependency Management Strategy

#### macOS (Existing)
- **Framework**: Universal Syphon.framework bundled in project
- **Build**: Automatic framework linking via Cargo.toml
- **Distribution**: Framework included in application bundle

#### Windows (New)
- **SDK**: Spout2 SDK via git submodule or vcpkg
- **Build**: C++ compilation via build.rs and cc crate
- **Distribution**: Spout2.dll bundled with application

### Performance Considerations

#### Zero-Copy Architecture
- **macOS**: IOSurface → OpenGL texture → Syphon
- **Windows**: D3D11 texture → Spout2 texture sharing
- **Both**: Hardware-accelerated, zero-copy performance

#### Memory Management
- **macOS**: Automatic reference counting (ARC) in Objective-C
- **Windows**: RAII in C++ with smart pointers
- **Rust**: Ownership system ensures memory safety on both platforms

### Build Commands (Cross-Platform)

#### Development Build
```bash
# macOS
cargo build

# Windows  
cargo build

# Cross-compilation (if needed)
cargo build --target x86_64-pc-windows-msvc     # From macOS/Linux to Windows
cargo build --target x86_64-apple-darwin        # From Windows/Linux to macOS
```

#### Production Build
```bash
# macOS
cargo build --release

# Windows
cargo build --release
```

## File Structure Changes

### New Files to Create
```
src-tauri/src/
├── spout_simple.rs           🆕 Windows Rust module
├── spout_bridge.cpp          🆕 Windows C++ bridge  
├── screencapture_d3d11.cpp   🆕 Windows screen capture
└── build.rs                  🆕 Cross-platform build script

lib/spout2/                   🆕 Spout2 SDK (git submodule)
├── SpoutLibrary.h
├── SpoutSender.h
└── SpoutSender.cpp
```

### Modified Files
```
src-tauri/
├── Cargo.toml               📝 Add Windows dependencies
└── src/main.rs              📝 Enhance Windows SpoutOutput implementation
```

## API Compatibility Matrix

### Unified Interface
| Method | macOS (Syphon) | Windows (Spout) | Status |
|--------|----------------|-----------------|---------|
| `new(name)` | ✅ | 🆕 | Unified |
| `update_frame()` | ✅ | 🆕 | Unified |
| `start_screen_capture()` | ✅ | 🆕 | Unified |
| `publish_screen_capture()` | ✅ | 🆕 | Unified |
| `has_clients()` | ✅ | 🆕 | Unified |
| `stop()` | ✅ | 🆕 | Unified |

### Platform-Specific Features
| Feature | macOS | Windows | Notes |
|---------|-------|---------|-------|
| Framework | Syphon.framework | Spout2 SDK | Different underlying tech |
| Texture API | OpenGL/Metal | D3D11 | Platform graphics APIs |
| Screen Capture | ScreenCaptureKit | DXGI | Platform capture APIs |
| Distribution | .framework bundle | .dll bundle | Platform packaging |

## Timeline & Milestones

### Week 1: Infrastructure
- ✅ Architecture analysis (completed)
- ✅ Implementation plan (completed)  
- 🎯 Create spout_bridge.cpp
- 🎯 Create spout_simple.rs
- 🎯 Update build system

### Week 2: Core Functionality
- 🎯 Implement basic Spout server creation
- 🎯 Add frame publishing capability
- 🎯 Test with Windows Spout receivers

### Week 3: Screen Capture
- 🎯 Implement DXGI desktop duplication
- 🎯 Add window-specific capture
- 🎯 Performance optimization

### Week 4: Testing & Polish
- 🎯 Cross-platform testing
- 🎯 Documentation updates
- 🎯 Example applications

## Success Criteria

### Functional Requirements
- ✅ Existing macOS Syphon functionality unchanged
- 🎯 Windows Spout server creation and publishing
- 🎯 Cross-platform API compatibility
- 🎯 Zero-copy performance on both platforms

### Performance Requirements
- 🎯 60 FPS video sharing on both platforms
- 🎯 <16ms latency for screen capture
- 🎯 Minimal CPU usage (hardware acceleration)

### Compatibility Requirements  
- 🎯 Works with existing Windows Spout receivers (OBS, TouchDesigner, etc.)
- 🎯 Maintains existing macOS Syphon receiver compatibility
- 🎯 Same JavaScript API on both platforms

## Risk Mitigation

### Technical Risks
- **Spout2 SDK Integration**: Use proven vcpkg or git submodule approach
- **D3D11 Complexity**: Start with basic texture sharing, add optimizations later
- **Build System**: Use established cc crate patterns for C++ compilation

### Compatibility Risks
- **API Changes**: Mirror existing Syphon interface exactly
- **Performance**: Use same zero-copy patterns as macOS implementation
- **Distribution**: Follow established Windows DLL bundling practices

## Conclusion

This plan leverages the existing conditional compilation infrastructure and mirrors the successful Syphon architecture for Windows Spout integration. The result will be a clean, maintainable cross-platform video sharing solution that preserves all existing functionality while adding robust Windows support.

**Key Benefits**:
- 🚫 **Zero Breaking Changes**: Existing Syphon code untouched
- 🔄 **Unified Development**: Same Tauri commands work on both platforms  
- 📐 **Architecture Consistency**: Proven patterns replicated for Windows
- ⚡ **Performance Parity**: Zero-copy D3D11 matches IOSurface performance
- 🛠️ **Maintainable**: Clear separation between platform implementations

The implementation follows established patterns and leverages the robust foundation already built for macOS Syphon integration.