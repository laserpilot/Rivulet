// Rivulet - Modern CEF-Spout Video Sharing Application
// application.cpp - Main application implementation

#include "application.h"
#include "d3d11_device.h"
#include "web_layer.h"
#include "spout_sender.h"

#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_client.h>

#include <iostream>
#include <windowsx.h>

namespace Rivulet {

Application::Application(HINSTANCE instance) 
    : instance_(instance)
    , window_(nullptr)
    , window_class_(L"RivuletMainWindow")
    , window_width_(1280)
    , window_height_(720)
    , initialized_(false)
    , should_exit_(false) {
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize() {
    std::cout << "Initializing Rivulet application..." << std::endl;
    
    if (!InitializeCEF()) {
        std::cerr << "❌ Failed to initialize CEF" << std::endl;
        return false;
    }
    
    if (!InitializeWindow()) {
        std::cerr << "❌ Failed to initialize window" << std::endl;
        return false;
    }
    
    if (!InitializeD3D11()) {
        std::cerr << "❌ Failed to initialize D3D11" << std::endl;
        return false;
    }
    
    if (!InitializeSpout()) {
        std::cerr << "❌ Failed to initialize Spout" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "✅ All components initialized successfully" << std::endl;
    return true;
}

int Application::Run() {
    if (!initialized_) {
        std::cerr << "❌ Application not initialized" << std::endl;
        return -1;
    }
    
    std::cout << "🎬 Starting message loop..." << std::endl;
    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
    
    MSG msg;
    while (!should_exit_) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                should_exit_ = true;
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // CEF message loop work
        CefDoMessageLoopWork();
        
        // Render frame if needed
        OnPaint();
        
        // Small sleep to prevent 100% CPU usage
        Sleep(1);
    }
    
    std::cout << "Message loop ended" << std::endl;
    return static_cast<int>(msg.wParam);
}

void Application::Shutdown() {
    if (!initialized_) return;
    
    std::cout << "Shutting down application..." << std::endl;
    
    // Shutdown components in reverse order
    spout_sender_.reset();
    web_layer_.reset();
    d3d11_device_.reset();
    
    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    
    // Shutdown CEF
    CefShutdown();
    
    initialized_ = false;
    std::cout << "✅ Application shutdown complete" << std::endl;
}

bool Application::InitializeCEF() {
    std::cout << "Initializing CEF..." << std::endl;
    
    // CEF settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;  // We'll handle message loop
    
    // Initialize CEF
    CefMainArgs main_args(instance_);
    if (!CefInitialize(main_args, settings, nullptr, nullptr)) {
        return false;
    }
    
    std::cout << "✅ CEF initialized" << std::endl;
    return true;
}

bool Application::InitializeWindow() {
    std::cout << "Creating main window..." << std::endl;
    
    // Register window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = StaticWindowProc;
    wcex.hInstance = instance_;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = window_class_.c_str();
    
    if (!RegisterClassExW(&wcex)) {
        return false;
    }
    
    // Create window
    window_ = CreateWindowW(
        window_class_.c_str(),
        L"Rivulet - CEF-Spout Video Sharing",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_width_, window_height_,
        nullptr, nullptr, instance_, this
    );
    
    if (!window_) {
        return false;
    }
    
    std::cout << "✅ Main window created" << std::endl;
    return true;
}

bool Application::InitializeD3D11() {
    std::cout << "Initializing D3D11 device..." << std::endl;
    
    try {
        d3d11_device_ = std::make_unique<D3D11Device>();
        if (!d3d11_device_->Initialize()) {
            return false;
        }
        
        std::cout << "✅ D3D11 device initialized" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "D3D11 initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool Application::InitializeSpout() {
    std::cout << "Initializing Spout sender..." << std::endl;
    
    try {
        spout_sender_ = std::make_unique<RivuletSpoutSender>();
        if (!spout_sender_->Initialize("Rivulet Output")) {
            return false;
        }
        
        std::cout << "✅ Spout sender initialized" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Spout initialization error: " << e.what() << std::endl;
        return false;
    }
}

void Application::OnPaint() {
    // This will be called for each frame
    // Eventually will render CEF content and send to Spout
    
    // For now, just validate our systems are working
    if (spout_sender_ && d3d11_device_) {
        // TODO: Implement actual rendering pipeline
        // CEF → D3D11 texture → Spout sender
    }
}

void Application::OnResize(int width, int height) {
    window_width_ = width;
    window_height_ = height;
    
    std::cout << "Window resized to " << width << "x" << height << std::endl;
    
    // TODO: Resize CEF browser and D3D11 resources
}

void Application::OnDestroy() {
    should_exit_ = true;
    PostQuitMessage(0);
}

LRESULT Application::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Clear background
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnResize(width, height);
            return 0;
        }

        case WM_DESTROY:
            OnDestroy();
            return 0;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK Application::StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Application* app = nullptr;
    
    if (message == WM_NCCREATE) {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        app = (Application*)pcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    } else {
        app = (Application*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (app) {
        return app->WindowProc(hwnd, message, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}

} // namespace Rivulet