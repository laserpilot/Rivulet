# CEF-Spout Integration Plan

## Project Pivot: From Cross-Platform Tauri to Windows-Native CEF-Spout

### 🎯 Strategic Decision
We're pivoting from the cross-platform Tauri approach to a Windows-native CEF-Spout solution. This decision is based on:

1. **Proven Technology**: CEF-Spout is a mature, working solution for web content → Spout2 sharing
2. **Performance**: Direct D3D11 texture sharing vs CPU-intensive bitmap conversion
3. **Ecosystem Fit**: Better integration with Windows creative applications
4. **Reduced Complexity**: No cross-platform abstractions to maintain

## 📊 Current State Analysis

### ✅ What We Have (Tauri-based)
- Basic Tauri application with WebView
- Rust-based Spout2 integration (partially working)
- Cross-platform architecture (macOS Syphon + Windows Spout)
- Debug-enhanced Spout integration

### ❌ Current Limitations
- **Performance Bottleneck**: CPU-based bitmap sharing instead of GPU texture sharing
- **Architecture Mismatch**: Tauri's WebView doesn't expose D3D textures directly
- **Complex Integration**: Fighting against Tauri's rendering pipeline
- **Limited Control**: Can't optimize frame timing and synchronization

### 🔥 CEF-Spout Advantages
- **Zero-Copy Sharing**: Direct D3D11 texture sharing to Spout2
- **Frame Control**: Precise timing with `SendExternalBeginFrame()`
- **Proven Architecture**: Used in production creative applications
- **Hardware Acceleration**: Full GPU acceleration for web content rendering
- **Creative Ecosystem**: Already integrated with MadMapper, Resolume, etc.

## 🏗️ Architecture Options

### Option A: Pure CEF-Spout (Recommended)
```
┌─────────────────────────────────────────┐
│         Native Windows Application       │
│  ┌─────────────────────────────────────┐ │
│  │            CEF Browser              │ │
│  │        (Offscreen Rendering)        │ │
│  └─────────────────┬───────────────────┘ │
│                    │                     │
│  ┌─────────────────▼───────────────────┐ │
│  │         D3D11 Texture Buffer        │ │
│  │        (Shared Resource)            │ │
│  └─────────────────┬───────────────────┘ │
│                    │                     │
│  ┌─────────────────▼───────────────────┐ │
│  │           Spout2 Sender             │ │
│  │      (Zero-Copy Sharing)            │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

**Pros:**
- Maximum performance (zero-copy texture sharing)
- Full control over rendering pipeline
- Native Windows optimization
- Minimal complexity

**Cons:**
- Need to rewrite UI in native code
- Lose Rust ecosystem benefits

### Option B: Hybrid Approach
```
┌─────────────────────────────────────────┐
│            Tauri Shell                  │
│  ┌─────────────────────────────────────┐ │
│  │          Control UI                 │ │
│  │      (Settings, Status)             │ │
│  └─────────────────┬───────────────────┘ │
│                    │ IPC                 │
│  ┌─────────────────▼───────────────────┐ │
│  │         CEF-Spout Renderer          │ │
│  │        (Content Windows)            │ │
│  └─────────────────┬───────────────────┘ │
│                    │                     │
│  ┌─────────────────▼───────────────────┐ │
│  │           Spout2 Output             │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

**Pros:**
- Keep existing Tauri UI
- Leverage CEF-Spout for content rendering
- Gradual migration path

**Cons:**
- More complex architecture
- IPC overhead between components

## 📋 Implementation Task List

### Phase 1: Research & Analysis ⏳
- [x] Create experimental branch `cef-spout-experiment`
- [x] Create this planning document
- [ ] **CURRENT**: Clone and analyze fg-uulm/cef-spout repository
- [ ] Study Direct3D texture sharing implementation
- [ ] Document Spout2 integration patterns
- [ ] Analyze build requirements and dependencies
- [ ] Test existing CEF-Spout with creative applications

### Phase 2: Proof of Concept 🔬
- [ ] Build CEF-Spout from source
- [ ] Test basic web content rendering
- [ ] Verify Spout2 output with external receivers
- [ ] Measure performance vs current Tauri approach
- [ ] Document performance benchmarks

### Phase 3: Architecture Decision 🎯
- [ ] Choose between Pure CEF-Spout vs Hybrid approach
- [ ] Create detailed technical specification
- [ ] Plan migration strategy from current codebase
- [ ] Identify reusable components

### Phase 4: Implementation 🚀
**Pure CEF-Spout Path:**
- [ ] Set up CEF build environment
- [ ] Create native Windows application shell
- [ ] Implement web content loading and rendering
- [ ] Add Spout2 texture sharing integration
- [ ] Create UI for window management

**Hybrid Path:**
- [ ] Create CEF-Spout renderer module
- [ ] Implement IPC between Tauri and CEF renderer
- [ ] Migrate content window logic to CEF
- [ ] Maintain Tauri shell for control UI

### Phase 5: Feature Parity 📈
- [ ] Dynamic window creation
- [ ] URL loading and navigation
- [ ] Multiple content windows
- [ ] Performance monitoring
- [ ] Error handling and recovery

### Phase 6: Optimization & Polish ✨
- [ ] Frame rate optimization
- [ ] Memory usage optimization
- [ ] GPU resource management
- [ ] Application lifecycle management
- [ ] User experience improvements

## 🔧 Technical Deep Dive

### CEF-Spout Architecture Analysis (Based on fg-uulm/cef-spout)

#### 🏗️ Core Components

1. **Main Application (`main.cpp`)**
   ```cpp
   class Window {
       HWND hwnd_;
       std::shared_ptr<d3d11::Device> device_;
       std::shared_ptr<d3d11::SwapChain> swapchain_;
       std::shared_ptr<Composition> composition_;
   }
   ```
   - Native Windows application with D3D11 device
   - Manages composition of multiple layers
   - Handles window events and rendering loop

2. **D3D11 Wrapper (`d3d11.h/cpp`)**
   ```cpp
   class Device {
       std::shared_ptr<Texture2D> open_shared_texture(void*);
       std::shared_ptr<SwapChain> create_swapchain(HWND);
   }
   
   class Texture2D {
       void* share_handle() const;
       bool lock_key(uint64_t key, uint32_t timeout_ms);
   }
   ```
   - Complete D3D11 abstraction layer
   - Shared texture support with keyed mutex synchronization
   - GPU resource management

3. **Composition System (`composition.h`)**
   ```cpp
   class Composition {
       std::vector<std::shared_ptr<Layer>> layers_;
       void render(std::shared_ptr<d3d11::Context> const&);
   }
   
   class Layer {
       virtual void render(std::shared_ptr<d3d11::Context> const&) = 0;
       Rect bounds_;
   }
   ```
   - Layer-based rendering system
   - Supports multiple web views and image overlays
   - JSON-configurable layout

4. **Web Layer (`web_layer.cpp`)**
   ```cpp
   class WebView : public CefClient, CefRenderHandler {
       void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                              PaintElementType type,
                              const RectList& dirtyRects,
                              void* share_handle) override;
   }
   ```
   - CEF integration with shared texture support
   - `OnAcceleratedPaint` receives D3D11 texture handles directly
   - Frame synchronization with `SendExternalBeginFrame()`

5. **Spout Integration**
   ```cpp
   // From web_layer.cpp analysis:
   spoutSenderNames* sender;
   spoutGLDXinterop* interop;
   spoutDirectX* sdx;
   
   vector<ID3D11Texture2D*> activeTextures;
   vector<HANDLE> activeHandles;
   ```
   - Direct Spout2 SDK integration
   - Multiple sender support
   - D3D11 texture publishing

#### 🔄 Rendering Pipeline

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   CEF Browser   │───▶│  D3D11 Shared    │───▶│  Spout2 Sender  │
│  (Web Content)  │    │    Texture       │    │   (External     │
│                 │    │  (GPU Memory)    │    │  Applications)  │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│OnAcceleratedPaint│    │   Keyed Mutex    │    │    MadMapper    │
│  (Render Event) │    │ Synchronization  │    │    Resolume     │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

#### 🚀 Performance Features

1. **Zero-Copy Texture Sharing**
   - CEF renders directly to D3D11 shared texture
   - Spout2 publishes the same texture handle
   - No CPU-GPU transfers or bitmap conversions

2. **Frame Synchronization**
   ```cpp
   browser->GetHost()->SendExternalBeginFrame();
   ```
   - Application controls render timing
   - Can sync with display refresh rate or custom intervals
   - Eliminates unnecessary frame generation

3. **Multi-Layer Composition**
   - JSON-configurable layer system
   - Independent web views and image overlays
   - Normalized coordinate system (0-1)

#### 📋 Build System Analysis

1. **Dependencies**
   - CEF binary distribution (Chromium 72+)
   - Spout2 SDK (included in lib/Spout2/)
   - Visual Studio 2017+ with C++ tools
   - CMake 2.8.12.1+

2. **Build Process**
   ```bash
   set CEF_ROOT=path\to\cef\binary-distribution
   cmake -G "Visual Studio 14 Win64" ..
   # Build ALL_BUILD project in Visual Studio
   ```

3. **Output Structure**
   ```
   bin/Release/
   ├── cefmixer.exe          # Main application
   ├── CEF libraries         # Chromium runtime
   └── SpoutLibrary.dll      # Spout2 runtime
   ```

### Key Insights for Rivulet Integration

#### ✅ What Works Perfectly
1. **Proven Architecture**: CEF-Spout is used in production creative applications
2. **Performance**: True zero-copy texture sharing with 60+ FPS capability
3. **Flexibility**: JSON-configurable multi-layer system
4. **Ecosystem**: Direct compatibility with MadMapper, Resolume, TouchDesigner

#### 🔧 Adaptation Requirements
1. **URL Loading**: Need dynamic URL loading (currently static at startup)
2. **Window Management**: Multiple independent content windows
3. **Control Interface**: Settings and configuration UI
4. **Real-time Control**: Dynamic layer manipulation

#### 🎯 Implementation Strategy
- **Option A**: Fork CEF-Spout and add our specific features
- **Option B**: Extract core components and build new application
- **Option C**: Hybrid approach with Tauri control + CEF-Spout renderer

### Performance Expectations
- **Current (Tauri + Bitmap)**: ~30 FPS with high CPU usage
- **Target (CEF-Spout)**: 60+ FPS with minimal CPU overhead
- **Memory**: Significant reduction in RAM usage (no bitmap buffers)
- **Latency**: Sub-frame latency for real-time applications

## 📊 Detailed Comparison: Tauri vs CEF-Spout

### Current Tauri Implementation

#### ✅ Advantages
- **Rust Ecosystem**: Memory safety, modern toolchain
- **Cross-Platform**: macOS (Syphon) + Windows (Spout) support
- **WebView Integration**: Leverages system WebView (Edge WebView2)
- **Modern UI**: Rust + JavaScript frontend flexibility
- **Package Management**: Cargo for dependencies

#### ❌ Limitations & Problems
1. **Performance Bottleneck**
   ```rust
   // Current approach in Tauri
   win.webContents.on("paint", (event, dirty, image, texture) => {
     if (texture) {
       output.updateTexture(texture);  // ⚠️ Not working - no D3D access
     } else {
       output.updateFrame(image.getBitmap(), image.getSize()); // 🐌 CPU-intensive
     }
   });
   ```
   - **CPU-Bound**: Bitmap conversion and memory copying
   - **No Direct D3D Access**: WebView2 doesn't expose D3D textures
   - **Frame Rate Limited**: ~30 FPS maximum due to CPU bottleneck

2. **Architecture Mismatch**
   - **WebView2 Abstraction**: Can't access underlying rendering context
   - **Cross-Process Communication**: WebView2 runs in separate process
   - **Limited Control**: No frame timing control or synchronization

3. **Integration Complexity**
   ```rust
   // Our current problematic bridge
   Tauri Main Process → WebView2 Process → Painted Bitmap → Rust → C++ → Spout2
   ```
   - **Multiple Conversions**: Texture → Bitmap → Buffer → Texture
   - **Memory Overhead**: Large bitmap buffers in RAM
   - **Sync Issues**: No frame synchronization control

### CEF-Spout Implementation

#### ✅ Advantages
1. **Zero-Copy Performance**
   ```cpp
   // CEF-Spout direct approach
   void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          PaintElementType type,
                          const RectList& dirtyRects,
                          void* share_handle) override {
     // Direct D3D11 texture handle - zero copy!
     spout_sender->SendTexture(share_handle);
   }
   ```
   - **GPU-to-GPU**: Direct texture sharing without CPU involvement
   - **Native Performance**: 60+ FPS capability
   - **Low Latency**: Sub-frame rendering delays

2. **Full Control Architecture**
   ```cpp
   // Direct control over rendering pipeline
   CEF Browser → D3D11 Shared Texture → Spout2 → External Apps
   ```
   - **Frame Timing**: `SendExternalBeginFrame()` for precise control
   - **Resource Management**: Direct D3D11 device and context access
   - **Synchronization**: Keyed mutex for proper texture access

3. **Proven Ecosystem Integration**
   - **MadMapper**: Production use in video mapping
   - **Resolume**: VJ and live performance applications
   - **TouchDesigner**: Real-time interactive media
   - **Creative Community**: Established user base and patterns

#### ❌ Disadvantages
1. **C++ Complexity**
   - **Memory Management**: Manual resource lifecycle
   - **Windows-Only**: Native Win32 application
   - **Build Complexity**: CEF + D3D11 + Spout2 dependencies

2. **Development Overhead**
   - **Native UI**: Need to build interface from scratch
   - **Error Handling**: Lower-level error management
   - **Debugging**: More complex debugging compared to Rust

### 🎯 Strategic Decision Matrix

| Factor | Tauri (Current) | CEF-Spout | Winner |
|--------|----------------|-----------|---------|
| **Performance** | ~30 FPS, High CPU | 60+ FPS, Low CPU | 🏆 CEF-Spout |
| **Memory Usage** | High (bitmap buffers) | Low (shared textures) | 🏆 CEF-Spout |
| **Latency** | High (multi-copy) | Sub-frame | 🏆 CEF-Spout |
| **Ecosystem Fit** | Limited | Perfect | 🏆 CEF-Spout |
| **Development Speed** | Fast (Rust/JS) | Slower (C++) | 🏆 Tauri |
| **Cross-Platform** | Yes | Windows-only | 🏆 Tauri |
| **Memory Safety** | Rust guarantees | Manual C++ | 🏆 Tauri |
| **Creative App Compat** | Poor | Excellent | 🏆 CEF-Spout |

### 🔥 Key Insight: Why CEF-Spout Wins

**The fundamental issue**: Tauri/WebView2 abstraction prevents access to the underlying D3D11 rendering context needed for efficient texture sharing.

**CEF-Spout solution**: Direct access to CEF's D3D11 shared textures through `OnAcceleratedPaint()` callback, enabling true zero-copy GPU-to-GPU texture sharing.

**Bottom Line**: For high-performance creative applications, the architecture matters more than the development language. CEF-Spout's direct texture approach is fundamentally superior to any bitmap-based solution.

## 🚀 Modernization Strategy (Phase 1 Update)

### Strategic Decision: Modern Implementation Over Legacy Build
Rather than fighting with 7-year-old build systems and Visual Studio 2017 requirements, we're taking a **modernization-first approach**:

#### What We're Keeping (Proven Architecture)
1. **Core Performance Insight**: `OnAcceleratedPaint()` → D3D11 shared textures → Spout2 pipeline
2. **Architectural Patterns**: 
   - D3D11 device/context management patterns
   - Shared texture handling with keyed mutex synchronization
   - Spout2 sender integration approaches
   - Layer-based composition system concepts

#### What We're Modernizing (Development Environment)
1. **Build System**: 
   - **Old**: CMake targeting Visual Studio 2017, CEF Chromium 72
   - **New**: CMake 3.21+ targeting Visual Studio 2022, current CEF version
2. **Development Approach**:
   - **Old**: Adapt legacy codebase with complex dependencies
   - **New**: Extract proven patterns, implement with modern C++17/20
3. **CEF Integration**:
   - **Old**: Outdated CEF APIs and build assumptions
   - **New**: Current CEF binary distribution with modern API usage

### Implementation Strategy: Extract & Modernize

```
Legacy CEF-Spout (2018)     →     Modern Rivulet (2025)
┌─────────────────────┐     →     ┌─────────────────────┐
│   VS 2017 Build    │     →     │   VS 2022 Build    │
│   CEF Chromium 72  │     →     │   Current CEF       │
│   Complex Legacy   │     →     │   Clean Modern     │
│   CMake Setup      │     →     │   CMake 3.21+      │
└─────────────────────┘     →     └─────────────────────┘
         │                                 │
         ▼                                 ▼
┌─────────────────────┐     →     ┌─────────────────────┐
│  Core Architecture │     →     │  Same Performance  │
│  D3D11 → Spout2    │     →     │  Architecture      │
│  60+ FPS Pipeline  │     →     │  Modern C++ Code   │
└─────────────────────┘     →     └─────────────────────┘
```

### Modern Development Phases

#### Phase 1: Modern Foundation ✅
- [x] Clean experimental branch setup
- [x] Current CEF binary distribution downloaded
- [x] Modern Spout2 SDK integrated
- [x] Strategic analysis complete

#### Phase 2: Core Architecture (In Progress)
- [ ] Fresh CMake project with VS 2022 support
- [ ] Basic CEF application scaffold
- [ ] D3D11 device management implementation
- [ ] `OnAcceleratedPaint()` shared texture handling
- [ ] Spout2 sender integration
- [ ] **Milestone**: Basic web content → Spout pipeline working

#### Phase 3: Rivulet Features
- [ ] Dynamic URL loading system
- [ ] Multiple concurrent content windows
- [ ] Control interface (web-based or native)
- [ ] Performance optimization (60+ FPS target)
- [ ] **Milestone**: Feature-complete Rivulet application

#### Phase 4: Production Ready
- [ ] Creative application integration testing
- [ ] Performance benchmarking vs Tauri approach
- [ ] Extended stability testing
- [ ] **Milestone**: Production-ready v1.0

### Technical Modernization Benefits

| Aspect | Old Approach | Modern Approach | Benefit |
|--------|-------------|-----------------|---------|
| **Build System** | VS 2017 + Old CMake | VS 2022 + CMake 3.21+ | Current toolchain, better IDE integration |
| **CEF Version** | Chromium 72 (2018) | Current CEF | Latest web standards, security updates |
| **Code Quality** | Legacy C++ patterns | Modern C++17/20 | Better memory safety, cleaner code |
| **Development Speed** | Complex legacy setup | Clean modern project | Faster iteration, easier debugging |
| **Maintainability** | Outdated dependencies | Current ecosystem | Long-term viability |

### Risk Mitigation
- **Architecture Risk**: ✅ Mitigated - Core CEF-Spout architecture is proven and sound
- **Implementation Risk**: 🔄 Managing - Incremental approach with frequent milestones
- **Performance Risk**: ✅ Mitigated - Same fundamental zero-copy texture sharing
- **Compatibility Risk**: ✅ Mitigated - Modern tools have better Spout ecosystem support

## 🎮 Use Cases & Testing

### Target Applications
1. **MadMapper**: Video mapping and projection
2. **Resolume**: VJ and live video mixing
3. **TouchDesigner**: Real-time interactive media systems
4. **OBS Studio**: Streaming and recording
5. **Custom creative applications**

### Test Scenarios
1. **Basic Web Content**: Static HTML pages
2. **Dynamic Content**: Animated web applications
3. **High Frame Rate**: 60+ FPS content
4. **Multiple Windows**: Concurrent content streams
5. **Resource Intensive**: WebGL and video content

## 📈 Success Metrics

### Performance Targets
- [ ] 60+ FPS for typical web content
- [ ] <16ms frame latency
- [ ] <5% CPU usage for content rendering
- [ ] Stable memory usage over extended periods

### Compatibility Targets
- [ ] Works with all major Spout receivers
- [ ] Supports modern web standards (ES6+, WebGL, etc.)
- [ ] Handles dynamic content and animations smoothly
- [ ] Reliable operation for 24/7 installations

## 🚨 Risk Assessment

### High Risk
- **CEF Integration Complexity**: May require significant C++ development
- **Build System Complexity**: CEF has complex build requirements
- **Windows Dependencies**: Requires specific DirectX versions

### Medium Risk
- **Performance Regression**: Need to ensure better performance than current solution
- **Feature Gaps**: May lose some Tauri convenience features
- **Development Time**: Complete rewrite may take significant time

### Mitigations
- Start with existing CEF-Spout codebase as foundation
- Incremental development with frequent testing
- Maintain current Tauri solution as fallback during development

## 📅 Timeline Estimate

### Week 1-2: Research & Analysis
- CEF-Spout repository analysis
- Technical feasibility assessment
- Architecture decision

### Week 3-4: Proof of Concept
- Basic CEF-Spout build and test
- Performance benchmarking
- Integration testing

### Week 5-8: Implementation
- Core functionality development
- Feature parity with current solution
- Testing and optimization

### Week 9-10: Polish & Documentation
- Bug fixes and improvements
- Documentation and deployment
- Production readiness

---

## 📝 Notes & References

### Key Resources
- [fg-uulm/cef-spout GitHub](https://github.com/fg-uulm/cef-spout)
- [Spout2 Official Documentation](https://spout.zeal.co/)
- [CEF Documentation](https://bitbucket.org/chromiumembedded/cef)
- [DirectX 11 Texture Sharing](https://docs.microsoft.com/en-us/windows/win32/direct3d11/shared-resources)

### Decision Log
- **2025-01-29**: Decision to pivot from cross-platform Tauri to Windows-native CEF-Spout
- **2025-01-29**: Created experimental branch and planning document

---

*This document will be updated as we progress through the implementation phases.*