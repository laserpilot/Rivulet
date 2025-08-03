// Rivulet - Modern CEF-Spout Video Sharing Application
// application.cpp - Main application implementation

#include "application.h"
#include "d3d11_device.h"
#include "web_layer.h"
#include "spout_sender.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"

#include <iostream>
#include <windowsx.h>
#include <io.h>
#include <fcntl.h>

// Additional Windows headers for input handling
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM  
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#endif

namespace Rivulet {

Application::Application(HINSTANCE instance) 
    : instance_(instance)
    , window_(nullptr)
    , window_class_(L"RivuletMainWindow")
    , window_width_(1024)
    , window_height_(768)
    , control_panel_(nullptr)
    , back_button_(nullptr)
    , forward_button_(nullptr)
    , url_edit_(nullptr)
    , go_button_(nullptr)
    , control_panel_height_(40)
    , initialized_(false)
    , should_exit_(false)
    , left_mouse_down_(false)
    , right_mouse_down_(false) {
}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize() {
    std::cout << "🚀 Initializing Rivulet application..." << std::endl;
    
    if (!InitializeCEF()) {
        std::cerr << "❌ Failed to initialize CEF" << std::endl;
        return false;
    }
    
    if (!InitializeWindow()) {
        std::cerr << "❌ Failed to initialize window" << std::endl;
        return false;
    }
    
    if (!InitializeControls()) {
        std::cerr << "❌ Failed to initialize controls" << std::endl;
        return false;
    }
    
    if (!InitializeD3D11()) {
        std::cerr << "❌ Failed to initialize D3D11" << std::endl;
        return false;
    }
    
    if (!InitializeWebLayer()) {
        std::cerr << "❌ Failed to initialize web layer" << std::endl;
        return false;
    }
    
    if (!InitializeSpout()) {
        std::cerr << "❌ Failed to initialize Spout" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "✅ All components initialized successfully" << std::endl;
    std::cout << "🌐 Application ready - window should be visible with web content" << std::endl;
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
    int loop_count = 0;
    while (!should_exit_) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                std::cout << "Received WM_QUIT message, exiting..." << std::endl;
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
        
        // Debug output every 5 seconds
        loop_count++;
        if (loop_count % 5000 == 0) {
            std::cout << "Message loop running... (count: " << loop_count << ")" << std::endl;
        }
        
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
    
    // Set absolute cache path to avoid warnings
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string exe_dir = exe_path;
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));
    
    std::string cache_path = exe_dir + "\\cache";
    std::string log_path = exe_dir + "\\cef.log";
    
    CefString(&settings.cache_path).FromASCII(cache_path.c_str());
    CefString(&settings.log_file).FromASCII(log_path.c_str());
    settings.log_severity = LOGSEVERITY_INFO;
    
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

bool Application::InitializeControls() {
    std::cout << "Creating control panel..." << std::endl;
    
    // Create control panel as a child window with gray background
    control_panel_ = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | SS_GRAYFRAME,
        0, 0, window_width_, control_panel_height_,
        window_, nullptr, instance_, nullptr
    );
    
    if (!control_panel_) {
        return false;
    }
    
    // Create back button
    back_button_ = CreateWindowW(
        L"BUTTON", L"<",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 5, 30, 30,
        control_panel_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_BACK_BUTTON)), instance_, nullptr
    );
    
    // Create forward button
    forward_button_ = CreateWindowW(
        L"BUTTON", L">",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        40, 5, 30, 30,
        control_panel_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_FORWARD_BUTTON)), instance_, nullptr
    );
    
    // Create URL edit box
    url_edit_ = CreateWindowW(
        L"EDIT", L"https://www.google.com",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        75, 5, window_width_ - 150, 30,
        control_panel_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_URL_EDIT)), instance_, nullptr
    );
    
    // Create Go button
    go_button_ = CreateWindowW(
        L"BUTTON", L"Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        window_width_ - 65, 5, 60, 30,
        control_panel_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_GO_BUTTON)), instance_, nullptr
    );
    
    std::cout << "✅ Control panel created" << std::endl;
    return true;
}

bool Application::InitializeD3D11() {
    std::cout << "Initializing D3D11 device..." << std::endl;
    
    try {
        d3d11_device_ = std::make_shared<D3D11Device>();
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

bool Application::InitializeWebLayer() {
    std::cout << "Initializing web layer..." << std::endl;
    
    try {
        web_layer_ = std::make_unique<WebLayer>(d3d11_device_);
        
        // CEF browser should use full original size (content gets sent to Spout at this size)
        // but display will be offset by control panel
        int cef_width = 1024;
        int cef_height = 768;
        
        if (!web_layer_->Initialize("https://www.google.com", cef_width, cef_height)) {
            return false;
        }
        
        // Give initial focus to the browser
        web_layer_->SendFocusEvent(true);
        
        std::cout << "✅ Web layer initialized" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Web layer initialization error: " << e.what() << std::endl;
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
    // Render CEF content and send to Spout
    if (web_layer_ && web_layer_->HasNewFrame()) {
        const uint8_t* bitmap = web_layer_->GetBitmapBuffer();
        if (bitmap && spout_sender_) {
            // Send bitmap to Spout (BGRA format, 1024x768)
            spout_sender_->SendFrame(bitmap, 1024, 768);
        }
        
        // Trigger window repaint to show new content
        if (window_) {
            InvalidateRect(window_, nullptr, FALSE);
        }
    }
}

void Application::OnResize(int width, int height) {
    window_width_ = width;
    window_height_ = height;
    
    std::cout << "Window resized to " << width << "x" << height << std::endl;
    
    // Resize control panel
    if (control_panel_) {
        SetWindowPos(control_panel_, nullptr, 0, 0, width, control_panel_height_, SWP_NOZORDER);
        
        // Resize URL edit box
        if (url_edit_) {
            SetWindowPos(url_edit_, nullptr, 75, 5, width - 150, 30, SWP_NOZORDER);
        }
        
        // Reposition Go button
        if (go_button_) {
            SetWindowPos(go_button_, nullptr, width - 65, 5, 60, 30, SWP_NOZORDER);
        }
    }
    
    // TODO: Resize CEF browser if we want dynamic sizing
}

void Application::OnDestroy() {
    std::cout << "OnDestroy called - shutting down application" << std::endl;
    should_exit_ = true;
    PostQuitMessage(0);
}

LRESULT Application::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw CEF content if available
            if (web_layer_ && web_layer_->HasNewFrame()) {
                const uint8_t* bitmap = web_layer_->GetBitmapBuffer();
                if (bitmap) {
                    // Create bitmap info
                    BITMAPINFO bmi = {};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = 1024;
                    bmi.bmiHeader.biHeight = -768; // Negative for top-down
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    
                    // Draw the bitmap below the control panel, maintaining aspect ratio
                    int content_height = window_height_ - control_panel_height_;
                    
                    // Calculate aspect ratio preserving dimensions
                    float cef_aspect = 1024.0f / 768.0f;
                    float window_aspect = (float)window_width_ / (float)content_height;
                    
                    int draw_width, draw_height, draw_x, draw_y;
                    
                    if (window_aspect > cef_aspect) {
                        // Window is wider than CEF aspect ratio - fit to height
                        draw_height = content_height;
                        draw_width = (int)(draw_height * cef_aspect);
                        draw_x = (window_width_ - draw_width) / 2;
                        draw_y = control_panel_height_;
                    } else {
                        // Window is taller than CEF aspect ratio - fit to width  
                        draw_width = window_width_;
                        draw_height = (int)(draw_width / cef_aspect);
                        draw_x = 0;
                        draw_y = control_panel_height_ + (content_height - draw_height) / 2;
                    }
                    
                    // Clear the background first
                    RECT content_rect = {0, control_panel_height_, window_width_, window_height_};
                    FillRect(hdc, &content_rect, (HBRUSH)(COLOR_WINDOW + 1));
                    
                    // Draw the bitmap with proper aspect ratio
                    StretchDIBits(hdc, draw_x, draw_y, draw_width, draw_height,
                                 0, 0, 1024, 768,
                                 bitmap, &bmi, DIB_RGB_COLORS, SRCCOPY);
                } else {
                    // Clear background if no content
                    FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
                }
            } else {
                // Clear background if no content
                FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnResize(width, height);
            return 0;
        }

        // Mouse events
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // Only handle mouse events in the content area (below control panel)
            if (y >= control_panel_height_) {
                // Adjust y coordinate to content area and scale to CEF browser (1024x768)
                int content_y = y - control_panel_height_;
                int content_height = window_height_ - control_panel_height_;
                
                int cef_x = (x * 1024) / window_width_;
                int cef_y = (content_y * 768) / content_height;
                
                left_mouse_down_ = true;
                if (web_layer_) {
                    web_layer_->SendMouseClickEvent(cef_x, cef_y, true, false);
                }
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            if (y >= control_panel_height_) {
                int content_y = y - control_panel_height_;
                int content_height = window_height_ - control_panel_height_;
                
                int cef_x = (x * 1024) / window_width_;
                int cef_y = (content_y * 768) / content_height;
                
                left_mouse_down_ = false;
                if (web_layer_) {
                    web_layer_->SendMouseClickEvent(cef_x, cef_y, true, true);
                }
            }
            ReleaseCapture();
            return 0;
        }

        case WM_RBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            if (y >= control_panel_height_) {
                int content_y = y - control_panel_height_;
                int content_height = window_height_ - control_panel_height_;
                
                int cef_x = (x * 1024) / window_width_;
                int cef_y = (content_y * 768) / content_height;
                
                right_mouse_down_ = true;
                if (web_layer_) {
                    web_layer_->SendMouseClickEvent(cef_x, cef_y, false, false);
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            if (y >= control_panel_height_) {
                int content_y = y - control_panel_height_;
                int content_height = window_height_ - control_panel_height_;
                
                int cef_x = (x * 1024) / window_width_;
                int cef_y = (content_y * 768) / content_height;
                
                right_mouse_down_ = false;
                if (web_layer_) {
                    web_layer_->SendMouseClickEvent(cef_x, cef_y, false, true);
                }
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            if (y >= control_panel_height_) {
                int content_y = y - control_panel_height_;
                int content_height = window_height_ - control_panel_height_;
                
                int cef_x = (x * 1024) / window_width_;
                int cef_y = (content_y * 768) / content_height;
                
                if (web_layer_) {
                    // Always send mouse move events - CEF needs them for hover states
                    web_layer_->SendMouseMoveEvent(cef_x, cef_y);
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            
            // Convert screen coordinates to client coordinates
            POINT pt = {x, y};
            ScreenToClient(hwnd, &pt);
            
            // Scale to CEF coordinates
            int cef_x = (pt.x * 1024) / window_width_;
            int cef_y = (pt.y * 768) / window_height_;
            
            if (web_layer_) {
                web_layer_->SendMouseWheelEvent(cef_x, cef_y, 0, delta);
            }
            return 0;
        }

        // Keyboard events
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            // Check if Enter key pressed in URL edit box
            if (wParam == VK_RETURN && GetFocus() == url_edit_) {
                OnGoButton();
                return 0;
            }
            
            // Only send keyboard events to CEF if not in control area
            HWND focused = GetFocus();
            if (focused != url_edit_ && web_layer_) {
                web_layer_->SendKeyEvent(static_cast<int>(wParam), false);
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            HWND focused = GetFocus();
            if (focused != url_edit_ && web_layer_) {
                web_layer_->SendKeyEvent(static_cast<int>(wParam), true);
            }
            return 0;
        }

        case WM_CHAR: {
            HWND focused = GetFocus();
            if (focused != url_edit_ && web_layer_) {
                web_layer_->SendKeyEvent(static_cast<int>(wParam), false, true);
            }
            return 0;
        }

        // Control commands
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_BACK_BUTTON:
                    OnBackButton();
                    return 0;
                case ID_FORWARD_BUTTON:
                    OnForwardButton();
                    return 0;
                case ID_GO_BUTTON:
                    OnGoButton();
                    return 0;
                case ID_URL_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // URL edit box content changed
                    }
                    return 0;
                default:
                    break;
            }
            break;
        }

        // Focus events
        case WM_SETFOCUS: {
            if (web_layer_) {
                web_layer_->SendFocusEvent(true);
            }
            return 0;
        }

        case WM_KILLFOCUS: {
            if (web_layer_) {
                web_layer_->SendFocusEvent(false);
            }
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

// Control event handlers
void Application::OnBackButton() {
    if (web_layer_ && web_layer_->GetBrowser()) {
        web_layer_->GetBrowser()->GoBack();
        std::cout << "Navigation: Back" << std::endl;
    }
}

void Application::OnForwardButton() {
    if (web_layer_ && web_layer_->GetBrowser()) {
        web_layer_->GetBrowser()->GoForward();
        std::cout << "Navigation: Forward" << std::endl;
    }
}

void Application::OnGoButton() {
    if (!url_edit_ || !web_layer_) {
        std::cout << "OnGoButton: Missing components" << std::endl;
        return;
    }
    
    // Get URL from edit box
    wchar_t url_buffer[1024];
    int len = GetWindowTextW(url_edit_, url_buffer, sizeof(url_buffer) / sizeof(wchar_t));
    
    if (len == 0) {
        std::cout << "OnGoButton: Empty URL" << std::endl;
        return;
    }
    
    // Convert to UTF-8
    char utf8_url[1024];
    int result = WideCharToMultiByte(CP_UTF8, 0, url_buffer, -1, utf8_url, sizeof(utf8_url), nullptr, nullptr);
    
    if (result == 0) {
        std::cout << "OnGoButton: Failed to convert URL" << std::endl;
        return;
    }
    
    std::string url(utf8_url);
    std::cout << "OnGoButton: Attempting to load URL: " << url << std::endl;
    
    // Add http:// if no protocol specified
    if (url.find("://") == std::string::npos) {
        url = "https://" + url;
        std::cout << "OnGoButton: Added https prefix: " << url << std::endl;
    }
    
    web_layer_->LoadURL(url);
    std::cout << "OnGoButton: LoadURL called successfully" << std::endl;
}

void Application::OnUrlEnter() {
    OnGoButton(); // Same as clicking Go button
}

} // namespace Rivulet