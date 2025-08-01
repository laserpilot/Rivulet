# Spout2 SDK Integration Guide

## Overview
This guide explains how to integrate the actual Spout2 SDK with our existing Windows implementation. The current implementation provides a complete architecture foundation with placeholder functionality that can be replaced with real Spout2 SDK calls.

## Current Implementation Status

### ✅ **Architecture Complete**
- **`spout_bridge.cpp`** - C++ bridge with placeholder Spout2 calls
- **`spout_simple.rs`** - Rust interface matching Syphon API
- **`screencapture_d3d11.cpp`** - DXGI Desktop Duplication working
- **Build System** - Cross-platform compilation configured

### 🔄 **Placeholder Functions Ready for SDK Integration**
All placeholder functions in `spout_bridge.cpp` are marked with TODO comments and ready for replacement.

## Spout2 SDK Integration Steps

### Step 1: Obtain Spout2 SDK

#### Option A: Official Spout2 Release
```bash
# Download from official Spout repository
git clone https://github.com/leadedge/Spout2.git
# Or download release from: https://github.com/leadedge/Spout2/releases
```

#### Option B: vcpkg Package Manager
```bash
# Install via vcpkg (if available)
vcpkg install spout2
```

#### Option C: Pre-built Libraries
Download pre-built Spout2 libraries and headers for your target architecture.

### Step 2: Project Structure Setup

Create the following directory structure:
```
src-tauri/
├── lib/
│   └── spout2/
│       ├── include/
│       │   ├── SpoutLibrary.h
│       │   ├── SpoutSender.h
│       │   ├── SpoutReceiver.h
│       │   └── SpoutCommon.h
│       ├── lib/
│       │   ├── x64/
│       │   │   ├── SpoutLibrary.lib
│       │   │   └── SpoutLibrary.dll
│       │   └── x86/
│       │       ├── SpoutLibrary.lib
│       │       └── SpoutLibrary.dll
│       └── src/ (optional - for building from source)
```

### Step 3: Update Build System

#### Update `Cargo.toml`
```toml
[target.'cfg(target_os = "windows")'.dependencies]
winapi = { version = "0.3", features = [
    "d3d11", "dxgi", "dxgitype", "dxgiformat", 
    "winuser", "processthreadsapi", "handleapi"
] }

[build-dependencies]
cc = "1.0"
```

#### Update `build.rs`
```rust
#[cfg(target_os = "windows")]
fn build_windows_components() {
    println!("🪟 Building Windows Spout components...");
    
    // Compile C++ Spout bridge with SDK headers
    cc::Build::new()
        .cpp(true)
        .file("src/spout_bridge.cpp")
        .file("src/screencapture_d3d11.cpp")
        .include("lib/spout2/include")  // Add Spout2 headers
        .flag("/std:c++17")
        .flag("/EHsc")
        .compile("spout_bridge");
    
    // Link Spout2 library
    println!("cargo:rustc-link-search=native=lib/spout2/lib/x64");
    println!("cargo:rustc-link-lib=SpoutLibrary");
    
    // Link Windows system libraries
    println!("cargo:rustc-link-lib=d3d11");
    println!("cargo:rustc-link-lib=dxgi");
    println!("cargo:rustc-link-lib=dxguid");
    println!("cargo:rustc-link-lib=user32");
    println!("cargo:rustc-link-lib=kernel32");
}
```

### Step 4: Replace Placeholder Implementation

#### Update `spout_bridge.cpp` Header
```cpp
// Add Spout2 SDK headers
#include "SpoutLibrary.h"
#include "SpoutSender.h"

// Remove placeholder comments and implement real functionality
```

#### Key Functions to Update

##### 1. **`spout_server_create()`**
```cpp
// BEFORE (Placeholder)
SpoutServerState* spout_server_create(const char* name, void* d3d_device_ptr) {
    // Placeholder success - remove when Spout2 SDK is integrated
    state->initialized = true;
    std::cout << "✅ Spout server state created (placeholder): " << name << std::endl;
    return state;
}

// AFTER (Real Spout2 SDK)
SpoutServerState* spout_server_create(const char* name, void* d3d_device_ptr) {
    SpoutServerState* state = new SpoutServerState();
    state->name = new std::string(name);
    
    try {
        // Create real Spout sender
        state->spout_sender = new SpoutSender();
        
        // Set D3D11 device if provided
        if (d3d_device_ptr) {
            static_cast<SpoutSender*>(state->spout_sender)->SetD3DDevice(
                static_cast<ID3D11Device*>(d3d_device_ptr)
            );
        }
        
        // Create sender with initial size
        if (!static_cast<SpoutSender*>(state->spout_sender)->CreateSender(name, 1920, 1080)) {
            cleanup_spout_state(state);
            return nullptr;
        }
        
        state->initialized = true;
        std::cout << "✅ Real Spout sender created: " << name << std::endl;
        return state;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to create Spout sender: " << e.what() << std::endl;
        cleanup_spout_state(state);
        return nullptr;
    }
}
```

##### 2. **`spout_server_publish_frame()`**
```cpp
// BEFORE (Placeholder)
bool spout_server_publish_frame(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height) {
    // Placeholder success - simulate frame publishing
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 60 == 0) {
        std::cout << "📋 Spout frame published (placeholder): " << width << "x" << height << std::endl;
    }
    return true;
}

// AFTER (Real Spout2 SDK)
bool spout_server_publish_frame(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height) {
    if (!state || !state->initialized || !data) return false;
    
    try {
        SpoutSender* sender = static_cast<SpoutSender*>(state->spout_sender);
        
        // Create D3D11 texture from RGBA data
        ComPtr<ID3D11Texture2D> texture;
        if (!create_texture_from_rgba(state, data, width, height, &texture)) {
            return false;
        }
        
        // Send texture to Spout
        bool success = sender->SendTexture(texture.Get(), width, height);
        
        if (success) {
            state->has_clients = sender->GetSenderCount() > 0;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error publishing Spout frame: " << e.what() << std::endl;
        return false;
    }
}
```

##### 3. **`spout_server_has_clients()`**
```cpp
// BEFORE (Placeholder)
bool spout_server_has_clients(SpoutServerState* state) {
    if (!state || !state->initialized) return false;
    // Return placeholder status
    return state->has_clients;
}

// AFTER (Real Spout2 SDK)
bool spout_server_has_clients(SpoutServerState* state) {
    if (!state || !state->initialized) return false;
    
    try {
        SpoutSender* sender = static_cast<SpoutSender*>(state->spout_sender);
        state->has_clients = sender->GetSenderCount() > 0;
        return state->has_clients;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error checking Spout clients: " << e.what() << std::endl;
        return false;
    }
}
```

##### 4. **`spout_server_stop()`**
```cpp
// BEFORE (Placeholder)
void spout_server_stop(SpoutServerState* state) {
    if (!state) return;
    cleanup_spout_state(state);
    delete state;
}

// AFTER (Real Spout2 SDK)
void spout_server_stop(SpoutServerState* state) {
    if (!state) return;
    
    if (state->spout_sender) {
        try {
            SpoutSender* sender = static_cast<SpoutSender*>(state->spout_sender);
            sender->ReleaseSender();
            delete sender;
            state->spout_sender = nullptr;
        } catch (const std::exception& e) {
            std::cerr << "❌ Error stopping Spout sender: " << e.what() << std::endl;
        }
    }
    
    cleanup_spout_state(state);
    delete state;
    std::cout << "✅ Spout sender stopped and cleaned up" << std::endl;
}
```

### Step 5: Screen Capture Integration

#### Update `screencapture_d3d11.cpp` to Send to Spout
```cpp
// In spout_server_publish_screen_capture()
bool spout_server_publish_screen_capture(void* spout_server_state) {
    if (!g_capture_active.load()) return false;
    
    SpoutServerState* state = static_cast<SpoutServerState*>(spout_server_state);
    if (!state || !state->initialized) return false;
    
    // Get latest captured frame
    ComPtr<ID3D11Texture2D> frame_to_publish;
    {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        if (!g_latest_frame) return false;
        frame_to_publish = g_latest_frame;
    }
    
    try {
        SpoutSender* sender = static_cast<SpoutSender*>(state->spout_sender);
        
        // Get texture description
        D3D11_TEXTURE2D_DESC desc;
        frame_to_publish->GetDesc(&desc);
        
        // Send D3D11 texture directly to Spout
        bool success = sender->SendTexture(frame_to_publish.Get(), desc.Width, desc.Height);
        
        if (success) {
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 60 == 0) {
                std::cout << "📋 Screen frame sent to Spout: " << desc.Width << "x" << desc.Height 
                         << " Frame #" << frame_count << std::endl;
            }
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error sending screen frame to Spout: " << e.what() << std::endl;
        return false;
    }
}
```

### Step 6: Testing and Validation

#### Test Applications
1. **Spout Receiver Test** - Use SpoutCam or SpoutSender demo to verify output
2. **OBS Studio** - Test Spout source integration
3. **TouchDesigner** - Test real-time video processing
4. **MadMapper** - Test projection mapping integration

#### Testing Commands
```bash
# Build with real Spout2 SDK
cd src-tauri
cargo build --release

# Run application and test Spout output
cargo run

# Verify Spout sender is visible in Spout directory
# Check with SpoutSettings application
```

### Step 7: Distribution

#### Include Required DLLs
```
target/release/
├── your-app.exe
├── SpoutLibrary.dll     # Required Spout2 runtime
└── d3d11.dll           # Windows system DLL (usually present)
```

#### Installer Considerations
- Ensure SpoutLibrary.dll is in the same directory as your executable
- Consider using Visual C++ Redistributable if required by Spout2
- Test on clean Windows systems without development tools

## Troubleshooting

### Common Issues

#### 1. **Spout2 SDK Not Found**
```
Error: SpoutLibrary.h not found
```
**Solution**: Verify SDK path in build.rs and ensure headers are in `lib/spout2/include/`

#### 2. **Linking Errors**
```
Error: unresolved external symbol SpoutSender::CreateSender
```
**Solution**: Ensure SpoutLibrary.lib is linked and DLL is available at runtime

#### 3. **D3D11 Device Issues**
```
Error: Failed to create Spout sender
```
**Solution**: Verify D3D11 device creation and compatibility with Spout2

#### 4. **Performance Issues**
- Ensure hardware acceleration is enabled
- Check D3D11 device feature level compatibility
- Verify frame rate limiting (avoid over-publishing)

### Debug Tools

#### Enable Spout Logging
```cpp
// In spout_bridge.cpp initialization
#ifdef _DEBUG
    // Enable Spout debug logging if available
    SpoutLog::Instance().SetLevel(SPOUT_LOG_VERBOSE);
#endif
```

#### Windows Event Viewer
Check Windows Event Viewer for D3D11 and DXGI related errors under:
- Windows Logs → Application
- Applications and Services Logs → Microsoft → Windows → DXGI

## Integration Checklist

### Pre-Integration
- [ ] Spout2 SDK downloaded and verified
- [ ] Directory structure created (`lib/spout2/`)
- [ ] Build system paths configured
- [ ] Test environment prepared

### Implementation
- [ ] Replace `spout_server_create()` placeholder
- [ ] Replace `spout_server_publish_frame()` placeholder  
- [ ] Replace `spout_server_has_clients()` placeholder
- [ ] Replace `spout_server_stop()` placeholder
- [ ] Update screen capture publishing
- [ ] Add error handling and logging

### Testing
- [ ] Build succeeds with Spout2 SDK
- [ ] Basic Spout sender creation works
- [ ] Frame publishing tested with receiver
- [ ] Screen capture integration verified
- [ ] Performance benchmarked
- [ ] Memory leaks checked

### Distribution
- [ ] Required DLLs identified and included
- [ ] Installation package tested
- [ ] Clean system deployment verified
- [ ] Cross-platform build still works (macOS unchanged)

## Expected Results

After successful integration:

1. **Real Spout Output**: Applications like OBS Studio will detect your Spout sender
2. **Zero-Copy Performance**: D3D11 textures shared directly without CPU copies  
3. **Full Compatibility**: Works with all existing Spout receivers
4. **Cross-Platform**: macOS Syphon functionality remains unchanged
5. **Production Ready**: Suitable for real-world video sharing applications

The integration replaces placeholder functionality with production-quality Spout2 SDK calls while maintaining the same API and architecture patterns established in the foundation implementation.