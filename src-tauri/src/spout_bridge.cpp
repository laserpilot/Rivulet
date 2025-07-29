// Windows Spout2 C++ bridge - mirrors syphon_bridge.m functionality
// Provides C interface for Rust FFI integration

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <iostream>

// OpenGL constants for pixel formats
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

// Real Spout2 SDK headers
#include "SpoutLibrary.h"

using Microsoft::WRL::ComPtr;

// Define a C-compatible struct for the Spout server state
// Mirrors SyphonServerState from syphon_bridge.m
typedef struct {
    // Real Spout library interface (SPOUTHANDLE is SPOUTLIBRARY*)
    SPOUTHANDLE spout_library;
    // D3D11 device and context for texture operations
    ComPtr<ID3D11Device> d3d_device;
    ComPtr<ID3D11DeviceContext> d3d_context;
    // Server name for identification
    std::string* name;
    // Client connection status
    bool has_clients;
    // Initialization status
    bool initialized;
} SpoutServerState;

// Forward declarations for internal functions
static bool initialize_d3d11_device(SpoutServerState* state);
static bool create_texture_from_rgba(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height, ID3D11Texture2D** texture);
static void cleanup_spout_state(SpoutServerState* state);
static bool test_spout_dll_loading();

extern "C" {

// Create Spout server - mirrors syphon_server_create()
SpoutServerState* spout_server_create(const char* name, void* d3d_device_ptr) {
    std::cout << "🔧 SPOUT DEBUG: spout_server_create() called with name: " << (name ? name : "NULL") << std::endl;
    
    if (!name) {
        std::cerr << "❌ ERROR: spout_server_create received NULL name." << std::endl;
        return nullptr;
    }

    std::cout << "🔧 SPOUT DEBUG: Allocating SpoutServerState..." << std::endl;
    
    // Allocate server state
    SpoutServerState* state = new SpoutServerState();
    if (!state) {
        std::cerr << "❌ ERROR: Failed to allocate SpoutServerState." << std::endl;
        return nullptr;
    }

    std::cout << "🔧 SPOUT DEBUG: Initializing state structure..." << std::endl;
    
    // Initialize state
    state->spout_library = nullptr;
    state->name = new std::string(name);
    state->has_clients = false;
    state->initialized = false;

    std::cout << "🎬 Creating Spout sender: " << name << std::endl;

    // Initialize D3D11 device if not provided
    std::cout << "🔧 SPOUT DEBUG: Checking D3D11 device setup..." << std::endl;
    
    if (d3d_device_ptr) {
        std::cout << "🔧 SPOUT DEBUG: Using provided D3D11 device..." << std::endl;
        // Use provided D3D11 device
        state->d3d_device = static_cast<ID3D11Device*>(d3d_device_ptr);
        state->d3d_device->AddRef();
        state->d3d_device->GetImmediateContext(&state->d3d_context);
        std::cout << "✅ Using provided D3D11 device" << std::endl;
    } else {
        std::cout << "🔧 SPOUT DEBUG: Creating our own D3D11 device..." << std::endl;
        // Create our own D3D11 device
        if (!initialize_d3d11_device(state)) {
            std::cerr << "❌ ERROR: Failed to initialize D3D11 device" << std::endl;
            cleanup_spout_state(state);
            return nullptr;
        }
        std::cout << "✅ Created D3D11 device" << std::endl;
    }

    // Test DLL loading first
    std::cout << "🔧 SPOUT DEBUG: Testing SpoutLibrary.dll loading..." << std::endl;
    if (!test_spout_dll_loading()) {
        std::cerr << "❌ ERROR: SpoutLibrary.dll loading test failed" << std::endl;
        cleanup_spout_state(state);
        return nullptr;
    }
    
    // Initialize real Spout2 library interface
    std::cout << "🔧 SPOUT DEBUG: Starting Spout2 SDK initialization..." << std::endl;
    
    try {
        std::cout << "🔧 SPOUT DEBUG: Attempting to call GetSpout() factory function..." << std::endl;
        
        // Get Spout library instance using the factory function
        state->spout_library = GetSpout();
        
        std::cout << "🔧 SPOUT DEBUG: GetSpout() returned: " << (state->spout_library ? "SUCCESS" : "NULL") << std::endl;
        
        if (!state->spout_library) {
            std::cerr << "❌ ERROR: GetSpout() factory function returned NULL - Spout2 SDK failed to initialize" << std::endl;
            std::cerr << "❌ This could indicate:" << std::endl;
            std::cerr << "   - SpoutLibrary.dll not found or not loaded properly" << std::endl;
            std::cerr << "   - Missing Visual C++ runtime dependencies" << std::endl;
            std::cerr << "   - Incompatible SpoutLibrary.dll version" << std::endl;
            cleanup_spout_state(state);
            return nullptr;
        }
        
        std::cout << "🔧 SPOUT DEBUG: Setting sender name to: " << name << std::endl;
        // Set the sender name
        state->spout_library->SetSenderName(name);
        
        std::cout << "🔧 SPOUT DEBUG: Setting sender format to BGRA..." << std::endl;
        // Set sender format to BGRA (Windows native DXGI format)
        state->spout_library->SetSenderFormat(DXGI_FORMAT_B8G8R8A8_UNORM);
        
        state->initialized = true;
        std::cout << "✅ Real Spout2 SDK library created successfully: " << name << std::endl;
        std::cout << "🔧 SPOUT DEBUG: Spout initialization complete!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: C++ Exception during Spout library initialization: " << e.what() << std::endl;
        cleanup_spout_state(state);
        return nullptr;
    } catch (...) {
        std::cerr << "❌ ERROR: Unknown C++ exception during Spout library initialization" << std::endl;
        std::cerr << "❌ This suggests a critical failure in SpoutLibrary.dll" << std::endl;
        cleanup_spout_state(state);
        return nullptr;
    }

    return state;
}

// Publish RGBA frame - mirrors syphon_server_publish_frame()
bool spout_server_publish_frame(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height) {
    if (!state || !state->initialized || !data) {
        std::cerr << "❌ ERROR: Invalid parameters for spout_server_publish_frame" << std::endl;
        return false;
    }

    if (width == 0 || height == 0) {
        std::cerr << "❌ ERROR: Invalid dimensions: " << width << "x" << height << std::endl;
        return false;
    }

    if (!state->spout_library) {
        std::cerr << "❌ ERROR: Spout library not initialized" << std::endl;
        return false;
    }

    try {
        // Send image data to Spout using the real SDK
        // The Spout library will create/update the sender as needed
        // Note: Using GL_BGRA for DXGI captured frames (Windows native format)
        // bInvert = false since we want to maintain the original orientation
        bool success = state->spout_library->SendImage(data, width, height, GL_BGRA, false);
        
        if (success) {
            // Update client status using real Spout2 API
            int sender_count = state->spout_library->GetSenderCount();
            state->has_clients = (sender_count > 0);
            
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 60 == 0) { // Log every 60 frames
                std::cout << "📋 Real Spout2 frame published: " << width << "x" << height 
                          << " Frame #" << frame_count << " (Connected receivers: " << sender_count << ")" << std::endl;
            }
        } else {
            std::cerr << "❌ ERROR: Spout2 SDK SendImage() failed for " << width << "x" << height << " frame" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_publish_frame: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "❌ ERROR: Unknown exception in spout_server_publish_frame" << std::endl;
        return false;
    }
}

// Publish D3D11 texture directly - mirrors syphon_server_publish_iosurface()
bool spout_server_publish_texture(SpoutServerState* state, void* texture_handle) {
    if (!state || !state->initialized || !texture_handle) {
        std::cerr << "❌ ERROR: Invalid parameters for spout_server_publish_texture" << std::endl;
        return false;
    }

    // TODO: Implement direct D3D11 texture publishing with Spout2 SDK
    // The current SpoutLibrary.h interface primarily supports OpenGL textures and image data
    // For D3D11 texture support, we would need to either:
    // 1. Convert D3D11 texture to pixel data and use SendImage()
    // 2. Use a more advanced Spout2 interface if available
    
    try {
        // For now, log that this is not yet implemented
        std::cout << "⚠️ Direct D3D11 texture publishing not yet implemented - use SendImage() instead" << std::endl;
        
        // In a full implementation, we would:
        // 1. Map the D3D11 texture to get pixel data
        // 2. Call state->spout_library->SendImage() with the pixel data
        
        return false; // Return false to indicate this method is not implemented
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_publish_texture: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "❌ ERROR: Unknown exception in spout_server_publish_texture" << std::endl;
        return false;
    }
}

// Check if clients are connected - mirrors syphon_server_has_clients()
bool spout_server_has_clients(SpoutServerState* state) {
    if (!state || !state->initialized || !state->spout_library) {
        return false;
    }

    try {
        // Check if the sender has any connected receivers using real Spout2 API
        int receiver_count = state->spout_library->GetSenderCount();
        state->has_clients = (receiver_count > 0);
        return state->has_clients;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_has_clients: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "❌ ERROR: Unknown exception in spout_server_has_clients" << std::endl;
        return false;
    }
}

// Stop and cleanup Spout server - mirrors syphon_server_stop()
void spout_server_stop(SpoutServerState* state) {
    if (!state) return;

    std::cout << "🛑 Stopping Spout server: " << (state->name ? *state->name : "unknown") << std::endl;

    // Cleanup real Spout2 library
    if (state->spout_library) {
        try {
            // Release the Spout sender resources using real Spout2 API
            state->spout_library->ReleaseSender(0); // 0 = immediate release
            // Note: The SPOUTHANDLE itself is managed by the Spout2 library
            // We don't need to call Release() on it like a COM object
            state->spout_library = nullptr;
            std::cout << "✅ Spout2 SDK sender resources released" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ ERROR: Exception stopping Spout2 library: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "❌ ERROR: Unknown exception stopping Spout2 library" << std::endl;
        }
    }

    cleanup_spout_state(state);
    delete state;

    std::cout << "✅ Spout server stopped and cleaned up" << std::endl;
}

// Create test D3D11 texture - mirrors create_test_iosurface()
void* create_test_d3d11_texture(uint32_t width, uint32_t height) {
    // TODO: Implement test D3D11 texture creation for zero-copy testing
    // This would create a D3D11 texture with test pattern data
    // Currently not needed since we're using SendImage() path
    std::cout << "📋 create_test_d3d11_texture called: " << width << "x" << height << " (not implemented - use SendImage path)" << std::endl;
    return nullptr;
}

// Screen capture functions - implemented in screencapture_d3d11.cpp
bool spout_server_start_screen_capture();
bool spout_server_start_window_capture(uint32_t window_id);
bool spout_server_start_application_capture();
bool spout_server_start_application_window_capture(uint32_t window_id);
bool spout_server_start_content_only_window_capture(uint32_t window_id);
bool spout_server_has_screen_frame();
bool spout_server_publish_screen_capture(void* spout_server_state);
void spout_server_stop_screen_capture();

// Window detection functions - implemented in screencapture_d3d11.cpp
uint32_t get_frontmost_window_id();
uint32_t get_our_window_info();
uint32_t get_window_by_title_prefix(const char* prefix);

} // extern "C"

// Internal helper functions

static bool initialize_d3d11_device(SpoutServerState* state) {
    if (!state) return false;

    HRESULT hr;
    
    // Create D3D11 device and context
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        nullptr,                    // pAdapter
        D3D_DRIVER_TYPE_HARDWARE,   // DriverType
        nullptr,                    // Software
        create_device_flags,        // Flags
        feature_levels,             // pFeatureLevels
        ARRAYSIZE(feature_levels),  // FeatureLevels
        D3D11_SDK_VERSION,          // SDKVersion
        &state->d3d_device,         // ppDevice
        nullptr,                    // pFeatureLevel
        &state->d3d_context         // ppImmediateContext
    );

    if (FAILED(hr)) {
        std::cerr << "❌ ERROR: Failed to create D3D11 device. HRESULT: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    return true;
}

static bool create_texture_from_rgba(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height, ID3D11Texture2D** texture) {
    if (!state || !data || !texture) return false;

    // Create texture description
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    // Create initial data
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = data;
    init_data.SysMemPitch = width * 4; // 4 bytes per pixel (RGBA)
    init_data.SysMemSlicePitch = 0;

    // Create the texture
    HRESULT hr = state->d3d_device->CreateTexture2D(&desc, &init_data, texture);
    if (FAILED(hr)) {
        std::cerr << "❌ ERROR: Failed to create D3D11 texture. HRESULT: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    return true;
}

static void cleanup_spout_state(SpoutServerState* state) {
    if (!state) return;

    // Release D3D11 resources
    if (state->d3d_context) {
        state->d3d_context.Reset();
    }
    if (state->d3d_device) {
        state->d3d_device.Reset();
    }

    // Cleanup name string
    if (state->name) {
        delete state->name;
        state->name = nullptr;
    }

    // Note: spout_library cleanup is handled in spout_server_stop()
    // to avoid double-cleanup issues

    state->initialized = false;
}

// Test if SpoutLibrary.dll can be loaded
static bool test_spout_dll_loading() {
    std::cout << "🔧 SPOUT DLL TEST: Attempting to load SpoutLibrary.dll..." << std::endl;
    
    // Try to load the DLL using Windows LoadLibrary
    HMODULE spout_dll = LoadLibraryA("SpoutLibrary.dll");
    if (!spout_dll) {
        DWORD error = GetLastError();
        std::cerr << "❌ DLL TEST FAILED: Could not load SpoutLibrary.dll" << std::endl;
        std::cerr << "❌ Windows Error Code: " << error << std::endl;
        
        // Common error codes
        switch (error) {
            case 126: // ERROR_MOD_NOT_FOUND
                std::cerr << "❌ Error 126: The specified module could not be found." << std::endl;
                std::cerr << "❌ SpoutLibrary.dll is not in the current directory or PATH" << std::endl;
                break;
            case 127: // ERROR_PROC_NOT_FOUND  
                std::cerr << "❌ Error 127: The specified procedure could not be found." << std::endl;
                break;
            case 193: // ERROR_BAD_EXE_FORMAT
                std::cerr << "❌ Error 193: Invalid executable format (32-bit vs 64-bit mismatch)" << std::endl;
                break;
            default:
                std::cerr << "❌ Unknown DLL loading error" << std::endl;
                break;
        }
        return false;
    }
    
    std::cout << "✅ DLL TEST: SpoutLibrary.dll loaded successfully" << std::endl;
    
    // Test if GetSpout function exists
    typedef SPOUTHANDLE (WINAPI *GetSpoutFunc)(VOID);
    GetSpoutFunc getSpoutFunc = (GetSpoutFunc)GetProcAddress(spout_dll, "GetSpout");
    
    if (!getSpoutFunc) {
        std::cerr << "❌ DLL TEST FAILED: GetSpout function not found in SpoutLibrary.dll" << std::endl;
        FreeLibrary(spout_dll);
        return false;
    }
    
    std::cout << "✅ DLL TEST: GetSpout function found in SpoutLibrary.dll" << std::endl;
    
    // Clean up test
    FreeLibrary(spout_dll);
    std::cout << "✅ DLL TEST: All tests passed - SpoutLibrary.dll is ready" << std::endl;
    
    return true;
}