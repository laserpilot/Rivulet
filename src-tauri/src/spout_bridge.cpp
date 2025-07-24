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
// Note: Using SpoutLibrary interface instead of SpoutSender.h to avoid missing dependencies

using Microsoft::WRL::ComPtr;

// Define a C-compatible struct for the Spout server state
// Mirrors SyphonServerState from syphon_bridge.m
typedef struct {
    // Real Spout library interface
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

extern "C" {

// Create Spout server - mirrors syphon_server_create()
SpoutServerState* spout_server_create(const char* name, void* d3d_device_ptr) {
    if (!name) {
        std::cerr << "❌ ERROR: spout_server_create received NULL name." << std::endl;
        return nullptr;
    }

    // Allocate server state
    SpoutServerState* state = new SpoutServerState();
    if (!state) {
        std::cerr << "❌ ERROR: Failed to allocate SpoutServerState." << std::endl;
        return nullptr;
    }

    // Initialize state
    state->spout_library = nullptr;
    state->name = new std::string(name);
    state->has_clients = false;
    state->initialized = false;

    std::cout << "🎬 Creating Spout sender: " << name << std::endl;

    // Initialize D3D11 device if not provided
    if (d3d_device_ptr) {
        // Use provided D3D11 device
        state->d3d_device = static_cast<ID3D11Device*>(d3d_device_ptr);
        state->d3d_device->AddRef();
        state->d3d_device->GetImmediateContext(&state->d3d_context);
        std::cout << "✅ Using provided D3D11 device" << std::endl;
    } else {
        // Create our own D3D11 device
        if (!initialize_d3d11_device(state)) {
            std::cerr << "❌ ERROR: Failed to initialize D3D11 device" << std::endl;
            cleanup_spout_state(state);
            return nullptr;
        }
        std::cout << "✅ Created D3D11 device" << std::endl;
    }

    // Initialize real Spout2 library interface
    try {
        // Get Spout library instance
        state->spout_library = GetSpout();
        if (!state->spout_library) {
            std::cerr << "❌ ERROR: Failed to create Spout library instance" << std::endl;
            cleanup_spout_state(state);
            return nullptr;
        }
        
        // Set the sender name
        state->spout_library->SetSenderName(name);
        
        state->initialized = true;
        std::cout << "✅ Real Spout library interface created successfully: " << name << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception creating Spout library: " << e.what() << std::endl;
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
        // Send image data to Spout
        // The Spout library will create/update the sender as needed
        // Note: Using GL_BGRA for DXGI captured frames (Windows native format)
        bool success = state->spout_library->SendImage(data, width, height, GL_BGRA, false);
        
        if (success) {
            // Update client status
            state->has_clients = state->spout_library->GetSenderCount() > 0;
            
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 60 == 0) { // Log every 60 frames
                std::cout << "📋 Real Spout frame published: " << width << "x" << height 
                          << " Frame #" << frame_count << " (Clients: " << (state->has_clients ? "Yes" : "No") << ")" << std::endl;
            }
        } else {
            std::cerr << "❌ ERROR: Spout library SendImage() failed" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_publish_frame: " << e.what() << std::endl;
        return false;
    }
}

// Publish D3D11 texture directly - mirrors syphon_server_publish_iosurface()
bool spout_server_publish_texture(SpoutServerState* state, void* texture_handle) {
    if (!state || !state->initialized || !texture_handle) {
        std::cerr << "❌ ERROR: Invalid parameters for spout_server_publish_texture" << std::endl;
        return false;
    }

    // TODO: Implement direct texture publishing when SDK is integrated
    /*
    try {
        SpoutSender* sender = static_cast<SpoutSender*>(state->spout_sender);
        ID3D11Texture2D* texture = static_cast<ID3D11Texture2D*>(texture_handle);
        
        if (!sender->SendTexture(texture)) {
            std::cerr << "❌ ERROR: Failed to send D3D11 texture via Spout" << std::endl;
            return false;
        }

        // Update client status
        state->has_clients = sender->GetSenderCount() > 0;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_publish_texture: " << e.what() << std::endl;
        return false;
    }
    */

    // Placeholder success
    std::cout << "📋 Spout texture published (placeholder)" << std::endl;
    return true;
}

// Check if clients are connected - mirrors syphon_server_has_clients()
bool spout_server_has_clients(SpoutServerState* state) {
    if (!state || !state->initialized || !state->spout_library) {
        return false;
    }

    try {
        // Check if the sender has any connected clients
        int client_count = state->spout_library->GetSenderCount();
        state->has_clients = (client_count > 0);
        return state->has_clients;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_has_clients: " << e.what() << std::endl;
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
            // Release the Spout library resources
            state->spout_library->ReleaseSender();
            state->spout_library->Release(); // Release the library instance
            state->spout_library = nullptr;
            std::cout << "✅ Spout library resources released" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ ERROR: Exception stopping Spout library: " << e.what() << std::endl;
        }
    }

    cleanup_spout_state(state);
    delete state;

    std::cout << "✅ Spout server stopped and cleaned up" << std::endl;
}

// Create test D3D11 texture - mirrors create_test_iosurface()
void* create_test_d3d11_texture(uint32_t width, uint32_t height) {
    // This will be implemented when we add screen capture functionality
    // For now, return nullptr
    std::cout << "📋 create_test_d3d11_texture called: " << width << "x" << height << " (placeholder)" << std::endl;
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