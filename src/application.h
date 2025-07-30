// Rivulet - Modern CEF-Spout Video Sharing Application
// application.h - Main application class

#pragma once

#include <windows.h>
#include <memory>
#include <string>

// Forward declarations
namespace Rivulet {
    class D3D11Device;
    class WebLayer;
    class RivuletSpoutSender;
}

namespace Rivulet {

class Application {
public:
    explicit Application(HINSTANCE instance);
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    int Run();
    void Shutdown();

    // Window management
    LRESULT WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    static LRESULT CALLBACK StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    bool InitializeCEF();
    bool InitializeWindow();
    bool InitializeD3D11();
    bool InitializeSpout();
    
    void OnPaint();
    void OnResize(int width, int height);
    void OnDestroy();

    // Application instance
    HINSTANCE instance_;
    
    // Main window
    HWND window_;
    std::wstring window_class_;
    int window_width_;
    int window_height_;
    
    // Core components
    std::unique_ptr<D3D11Device> d3d11_device_;
    std::unique_ptr<WebLayer> web_layer_;
    std::unique_ptr<RivuletSpoutSender> spout_sender_;
    
    // State
    bool initialized_;
    bool should_exit_;
    
    // Default content URL
    static constexpr const char* kDefaultURL = "https://webglsamples.org/aquarium/aquarium.html";
};

} // namespace Rivulet