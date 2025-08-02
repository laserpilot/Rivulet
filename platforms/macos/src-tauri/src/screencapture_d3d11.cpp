// Windows screen capture implementation using DXGI Desktop Duplication
// Mirrors screencapture_iosurface.m functionality for cross-platform compatibility

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <chrono>

using Microsoft::WRL::ComPtr;

// Forward declaration for Spout server state
struct SpoutServerState;

// External function from spout_bridge.cpp
extern "C" bool spout_server_publish_frame(SpoutServerState* state, const uint8_t* data, uint32_t width, uint32_t height);

// Global capture state - mirrors the macOS implementation pattern
static ComPtr<ID3D11Device> g_d3d_device = nullptr;
static ComPtr<ID3D11DeviceContext> g_d3d_context = nullptr;
static ComPtr<IDXGIOutputDuplication> g_desktop_duplication = nullptr;
static ComPtr<ID3D11Texture2D> g_latest_frame = nullptr;
static std::mutex g_frame_mutex;
static std::atomic<bool> g_capture_active{false};
static std::thread g_capture_thread;
static std::atomic<bool> g_shutdown_requested{false};

// Forward declarations
static bool initialize_dxgi_desktop_duplication();
static bool setup_d3d11_device();
static void capture_thread_main();
static bool acquire_next_frame();
static void cleanup_capture_resources();
static HWND find_window_by_title_prefix(const std::wstring& prefix);
static HWND find_window_by_process_id(DWORD process_id);

extern "C" {

// Initialize desktop screen capture - mirrors syphon_server_start_screen_capture()
bool spout_server_start_screen_capture() {
    std::cout << "🎬 Starting Windows desktop screen capture (DXGI Desktop Duplication)" << std::endl;
    
    // Prevent multiple initialization
    if (g_capture_active.load()) {
        std::cout << "⚠️ Screen capture already active" << std::endl;
        return true;
    }
    
    // Setup D3D11 device if not already done
    if (!g_d3d_device) {
        if (!setup_d3d11_device()) {
            std::cerr << "❌ Failed to setup D3D11 device for screen capture" << std::endl;
            return false;
        }
    }
    
    // Initialize DXGI Desktop Duplication
    if (!initialize_dxgi_desktop_duplication()) {
        std::cerr << "❌ Failed to initialize DXGI Desktop Duplication" << std::endl;
        return false;
    }
    
    // Start capture thread
    g_shutdown_requested = false;
    g_capture_active = true;
    
    try {
        g_capture_thread = std::thread(capture_thread_main);
        std::cout << "✅ Windows screen capture started successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to start capture thread: " << e.what() << std::endl;
        g_capture_active = false;
        return false;
    }
}

// Initialize window-specific capture - mirrors syphon_server_start_window_capture()
bool spout_server_start_window_capture(uint32_t window_id) {
    std::cout << "🎯 Starting Windows window-specific capture for HWND: " << window_id << std::endl;
    
    // For now, we'll implement desktop capture with window filtering
    // Full window-specific capture would require Windows Graphics Capture API (Windows 10 1903+)
    // TODO: Implement proper window capture using Windows.Graphics.Capture
    
    std::cout << "⚠️ Window-specific capture not yet implemented - falling back to desktop capture" << std::endl;
    return spout_server_start_screen_capture();
}

// Initialize application-based capture - mirrors syphon_server_start_application_capture()
bool spout_server_start_application_capture() {
    std::cout << "🎯 Starting Windows application-based capture" << std::endl;
    
    // For Phase 2, we'll use desktop capture
    // Phase 3 would implement proper application filtering
    std::cout << "⚠️ Application-based capture not yet implemented - falling back to desktop capture" << std::endl;
    return spout_server_start_screen_capture();
}

// Initialize application window capture - mirrors Phase E.2 functionality
bool spout_server_start_application_window_capture(uint32_t window_id) {
    std::cout << "🎯 Starting Windows application window capture for HWND: " << window_id << std::endl;
    
    // Phase 2 implementation - desktop capture with window awareness
    std::cout << "⚠️ Application window capture not yet implemented - falling back to desktop capture" << std::endl;
    return spout_server_start_screen_capture();
}

// Initialize content-only window capture - mirrors Phase E.3 functionality
bool spout_server_start_content_only_window_capture(uint32_t window_id) {
    std::cout << "🎯 Starting Windows content-only window capture for HWND: " << window_id << std::endl;
    
    // Phase 2 implementation - desktop capture
    std::cout << "⚠️ Content-only window capture not yet implemented - falling back to desktop capture" << std::endl;
    return spout_server_start_screen_capture();
}

// Check if screen frames are available - mirrors syphon_server_has_screen_frame()
bool spout_server_has_screen_frame() {
    if (!g_capture_active.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(g_frame_mutex);
    return g_latest_frame != nullptr;
}

// Publish captured screen frame - mirrors syphon_server_publish_screen_capture()
bool spout_server_publish_screen_capture(void* spout_server_state) {
    if (!g_capture_active.load()) {
        return false;
    }
    
    if (!spout_server_state) {
        std::cerr << "❌ ERROR: Invalid Spout server state provided" << std::endl;
        return false;
    }
    
    // Get the latest captured frame
    ComPtr<ID3D11Texture2D> frame_to_publish;
    {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        if (!g_latest_frame) {
            return false; // No frame available
        }
        frame_to_publish = g_latest_frame;
    }
    
    try {
        // Get texture description for dimensions
        D3D11_TEXTURE2D_DESC desc;
        frame_to_publish->GetDesc(&desc);
        
        // Create a staging texture to read the data
        D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.BindFlags = 0;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        staging_desc.MiscFlags = 0;
        
        ComPtr<ID3D11Texture2D> staging_texture;
        HRESULT hr = g_d3d_device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
        if (FAILED(hr)) {
            std::cerr << "❌ ERROR: Failed to create staging texture for Spout publishing" << std::endl;
            return false;
        }
        
        // Copy frame to staging texture
        g_d3d_context->CopyResource(staging_texture.Get(), frame_to_publish.Get());
        
        // Map the staging texture to get pixel data
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        hr = g_d3d_context->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);
        if (FAILED(hr)) {
            std::cerr << "❌ ERROR: Failed to map staging texture for reading" << std::endl;
            return false;
        }
        
        // Note: DXGI captures in BGRA format, but Spout expects RGBA
        // For now, we'll send the raw data and let Spout handle format conversion
        // TODO: Add BGRA->RGBA conversion if needed
        
        // Publish frame data to Spout
        bool success = spout_server_publish_frame(
            static_cast<SpoutServerState*>(spout_server_state),
            static_cast<const uint8_t*>(mapped_resource.pData),
            desc.Width,
            desc.Height
        );
        
        // Unmap the staging texture
        g_d3d_context->Unmap(staging_texture.Get(), 0);
        
        if (success) {
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 60 == 0) { // Log every 60 frames (1 second at 60fps)
                std::cout << "📋 Windows screen frame published to Spout: " << desc.Width << "x" << desc.Height 
                          << " Frame #" << frame_count << std::endl;
            }
        } else {
            std::cerr << "❌ ERROR: Failed to publish screen frame to Spout" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: Exception in spout_server_publish_screen_capture: " << e.what() << std::endl;
        return false;
    }
}

// Stop screen capture - mirrors syphon_server_stop_screen_capture()
void spout_server_stop_screen_capture() {
    if (!g_capture_active.load()) {
        std::cout << "🛑 Screen capture already stopped" << std::endl;
        return;
    }
    
    std::cout << "🛑 Stopping Windows screen capture..." << std::endl;
    
    // Signal shutdown and wait for capture thread
    g_shutdown_requested = true;
    g_capture_active = false;
    
    if (g_capture_thread.joinable()) {
        g_capture_thread.join();
    }
    
    // Cleanup resources
    cleanup_capture_resources();
    
    std::cout << "✅ Windows screen capture stopped successfully" << std::endl;
}

// Window detection functions - mirrors macOS window detection

// Get frontmost window ID - mirrors get_frontmost_window_id()
uint32_t get_frontmost_window_id() {
    std::cout << "🔍 Getting frontmost window ID (Windows implementation)" << std::endl;
    
    HWND foreground_window = GetForegroundWindow();
    if (foreground_window == NULL) {
        std::cout << "⚠️ No foreground window found" << std::endl;
        return 0;
    }
    
    // Get window title for logging
    wchar_t window_title[256];
    GetWindowTextW(foreground_window, window_title, sizeof(window_title) / sizeof(wchar_t));
    
    // Get process ID
    DWORD process_id;
    GetWindowThreadProcessId(foreground_window, &process_id);
    
    // Don't return our own window
    if (process_id == GetCurrentProcessId()) {
        std::cout << "⚠️ Frontmost window is our own process, skipping" << std::endl;
        return 0;
    }
    
    std::wcout << L"✅ Found frontmost window: HWND=" << (uintptr_t)foreground_window 
               << L", Title=\"" << window_title << L"\"" << std::endl;
    
    return (uint32_t)(uintptr_t)foreground_window;
}

// Get our application window info - mirrors get_our_window_info()
uint32_t get_our_window_info() {
    std::cout << "🎯 Getting our application window info (Windows implementation)" << std::endl;
    
    DWORD our_process_id = GetCurrentProcessId();
    
    // Find our main window by enumerating windows
    HWND our_window = NULL;
    
    EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
        DWORD process_id;
        GetWindowThreadProcessId(hwnd, &process_id);
        
        if (process_id == (DWORD)lparam) {
            // Check if this is a main window (visible, has title)
            if (IsWindowVisible(hwnd)) {
                wchar_t window_title[256];
                if (GetWindowTextW(hwnd, window_title, sizeof(window_title) / sizeof(wchar_t)) > 0) {
                    // Found our main window
                    *((HWND*)lparam) = hwnd; // Store in lparam (hacky but works for demo)
                    return FALSE; // Stop enumeration
                }
            }
        }
        return TRUE; // Continue enumeration
    }, (LPARAM)our_process_id);
    
    // Recover the window handle (this is a hack for the demo)
    // In real code, we'd use a proper structure to pass data
    EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
        DWORD process_id;
        GetWindowThreadProcessId(hwnd, &process_id);
        
        if (process_id == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
            wchar_t window_title[256];
            if (GetWindowTextW(hwnd, window_title, sizeof(window_title) / sizeof(wchar_t)) > 0) {
                *((HWND*)lparam) = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&our_window);
    
    if (our_window) {
        wchar_t window_title[256];
        GetWindowTextW(our_window, window_title, sizeof(window_title) / sizeof(wchar_t));
        
        RECT rect;
        GetWindowRect(our_window, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        
        std::wcout << L"✅ Found our window: HWND=" << (uintptr_t)our_window 
                   << L", Title=\"" << window_title << L"\", Size=" << width << L"x" << height << std::endl;
        
        return (uint32_t)(uintptr_t)our_window;
    }
    
    std::cout << "⚠️ Could not find our application window" << std::endl;
    return 0;
}

// Get window by title prefix - mirrors get_window_by_title_prefix()
uint32_t get_window_by_title_prefix(const char* prefix) {
    std::cout << "🔍 Searching for window with title prefix: \"" << prefix << "\"" << std::endl;
    
    // Convert to wide string
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, prefix, -1, NULL, 0);
    std::vector<wchar_t> wide_prefix(wide_len);
    MultiByteToWideChar(CP_UTF8, 0, prefix, -1, wide_prefix.data(), wide_len);
    
    HWND found_window = find_window_by_title_prefix(wide_prefix.data());
    
    if (found_window) {
        wchar_t window_title[256];
        GetWindowTextW(found_window, window_title, sizeof(window_title) / sizeof(wchar_t));
        
        std::wcout << L"✅ Found window with prefix: HWND=" << (uintptr_t)found_window 
                   << L", Title=\"" << window_title << L"\"" << std::endl;
        
        return (uint32_t)(uintptr_t)found_window;
    }
    
    std::cout << "⚠️ No window found with title prefix: \"" << prefix << "\"" << std::endl;
    return 0;
}

} // extern "C"

// Internal implementation functions

static bool setup_d3d11_device() {
    std::cout << "🔧 Setting up D3D11 device for screen capture" << std::endl;
    
    HRESULT hr;
    
    // Create D3D11 device and context
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    
    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    hr = D3D11CreateDevice(
        nullptr,                    // pAdapter (use default)
        D3D_DRIVER_TYPE_HARDWARE,   // DriverType
        nullptr,                    // Software
        create_device_flags,        // Flags
        feature_levels,             // pFeatureLevels
        ARRAYSIZE(feature_levels),  // FeatureLevels
        D3D11_SDK_VERSION,          // SDKVersion
        &g_d3d_device,              // ppDevice
        nullptr,                    // pFeatureLevel
        &g_d3d_context              // ppImmediateContext
    );
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create D3D11 device for screen capture. HRESULT: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }
    
    std::cout << "✅ D3D11 device created successfully for screen capture" << std::endl;
    return true;
}

static bool initialize_dxgi_desktop_duplication() {
    std::cout << "🔧 Initializing DXGI Desktop Duplication" << std::endl;
    
    // Get DXGI device from D3D11 device
    ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = g_d3d_device.As(&dxgi_device);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to get DXGI device" << std::endl;
        return false;
    }
    
    // Get DXGI adapter
    ComPtr<IDXGIAdapter> dxgi_adapter;
    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to get DXGI adapter" << std::endl;
        return false;
    }
    
    // Get primary output (monitor)
    ComPtr<IDXGIOutput> dxgi_output;
    hr = dxgi_adapter->EnumOutputs(0, &dxgi_output);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to get primary DXGI output" << std::endl;
        return false;
    }
    
    // Get DXGI Output1 interface for desktop duplication
    ComPtr<IDXGIOutput1> dxgi_output1;
    hr = dxgi_output.As(&dxgi_output1);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to get DXGI Output1 interface" << std::endl;
        return false;
    }
    
    // Create desktop duplication
    hr = dxgi_output1->DuplicateOutput(g_d3d_device.Get(), &g_desktop_duplication);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create desktop duplication. HRESULT: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            std::cerr << "   Desktop duplication not available (may be in use by another application)" << std::endl;
        }
        return false;
    }
    
    std::cout << "✅ DXGI Desktop Duplication initialized successfully" << std::endl;
    return true;
}

static void capture_thread_main() {
    std::cout << "🎬 Windows screen capture thread started" << std::endl;
    
    while (!g_shutdown_requested.load()) {
        if (!acquire_next_frame()) {
            // Failed to acquire frame, wait a bit and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            continue;
        }
        
        // Target 60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    std::cout << "🏁 Windows screen capture thread ended" << std::endl;
}

static bool acquire_next_frame() {
    if (!g_desktop_duplication) {
        return false;
    }
    
    DXGI_OUTDUPL_FRAME_INFO frame_info;
    ComPtr<IDXGIResource> desktop_resource;
    
    // Try to acquire next frame
    HRESULT hr = g_desktop_duplication->AcquireNextFrame(0, &frame_info, &desktop_resource);
    
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // No new frame available (normal)
        return false;
    }
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to acquire next frame. HRESULT: 0x" 
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }
    
    // Get D3D11 texture from DXGI resource
    ComPtr<ID3D11Texture2D> acquired_texture;
    hr = desktop_resource.As(&acquired_texture);
    if (FAILED(hr)) {
        g_desktop_duplication->ReleaseFrame();
        return false;
    }
    
    // Store the frame for publishing
    {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        g_latest_frame = acquired_texture;
    }
    
    // Release the frame back to the system
    g_desktop_duplication->ReleaseFrame();
    
    return true;
}

static void cleanup_capture_resources() {
    std::cout << "🧹 Cleaning up Windows screen capture resources" << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(g_frame_mutex);
        g_latest_frame.Reset();
    }
    
    if (g_desktop_duplication) {
        g_desktop_duplication.Reset();
    }
    
    if (g_d3d_context) {
        g_d3d_context.Reset();
    }
    
    if (g_d3d_device) {
        g_d3d_device.Reset();
    }
    
    std::cout << "✅ Screen capture resources cleaned up" << std::endl;
}

static HWND find_window_by_title_prefix(const std::wstring& prefix) {
    HWND found_window = NULL;
    
    EnumWindows([](HWND hwnd, LPARAM lparam) -> BOOL {
        const std::wstring* prefix_ptr = (const std::wstring*)lparam;
        
        if (IsWindowVisible(hwnd)) {
            wchar_t window_title[256];
            if (GetWindowTextW(hwnd, window_title, sizeof(window_title) / sizeof(wchar_t)) > 0) {
                std::wstring title(window_title);
                if (title.find(*prefix_ptr) == 0) {
                    // Found match - store in a global or use a different approach
                    // For this demo, we'll use SetWindowLongPtr to store the result
                    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hwnd);
                    return FALSE; // Stop enumeration
                }
            }
        }
        return TRUE; // Continue enumeration
    }, (LPARAM)&prefix);
    
    // This is a simplified approach - in real code we'd use a proper callback structure
    return found_window;
}