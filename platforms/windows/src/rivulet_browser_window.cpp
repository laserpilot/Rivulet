// Rivulet - Modern CEF-Spout Video Sharing Application
// rivulet_browser_window.cpp - Professional browser window implementation
// Adapted from cefclient root_window_win.cc

#include "rivulet_browser_window.h"
#include "spout_sender.h"

#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"

#include <iostream>
#include <windowsx.h>
#include <fstream>
#include <shlobj.h>

namespace Rivulet {

namespace {
    const wchar_t kWindowClassName[] = L"RivuletBrowserWindow";
}

RivuletBrowserWindow::RivuletBrowserWindow(HINSTANCE instance)
    : instance_(instance)
    , hwnd_(nullptr)
    , window_class_(kWindowClassName)
    , window_width_(1024)
    , window_height_(768)
    , back_hwnd_(nullptr)
    , forward_hwnd_(nullptr)
    , reload_hwnd_(nullptr)
    , stop_hwnd_(nullptr)
    , edit_hwnd_(nullptr)
    , go_hwnd_(nullptr)
    , edit_wndproc_old_(nullptr)
    , resolution_wndproc_old_(nullptr)
    , font_(nullptr)
    , initialized_(false)
    , is_closing_(false)
    , toolbar_visible_(true)
    , spout_width_(1024)
    , spout_height_(768)
    , off_screen_dc_(nullptr)
    , off_screen_bitmap_(nullptr)
    , old_bitmap_(nullptr)
    , bitmap_pixels_(nullptr)
    , bitmap_width_(0)
    , bitmap_height_(0)
    , hardware_acceleration_enabled_(false)
    , use_synchronized_rendering_(true)
    , new_frame_ready_(false) {
    
    // Initialize critical section for thread synchronization
    InitializeCriticalSection(&bitmap_lock_);
    
    // Create frame synchronization event
    frame_ready_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

RivuletBrowserWindow::~RivuletBrowserWindow() {
    Shutdown();
    
    // Clean up synchronization objects
    if (frame_ready_event_) {
        CloseHandle(frame_ready_event_);
    }
    DeleteCriticalSection(&bitmap_lock_);
}

bool RivuletBrowserWindow::Initialize(const Config& config) {
    if (initialized_) return true;
    
    std::cout << "🚀 Initializing Rivulet Browser Window..." << std::endl;
    
    spout_width_ = config.spout_width;
    spout_height_ = config.spout_height;
    
    // Load saved settings (may override config defaults)
    LoadSettings();
    
    // Calculate window size to match Spout aspect ratio
    float spout_aspect = (float)spout_width_ / spout_height_;
    int desired_content_height = config.window_height;
    int calculated_content_width = (int)(desired_content_height * spout_aspect);
    
    // Set window dimensions (content area matches Spout aspect ratio)
    window_width_ = calculated_content_width;
    window_height_ = desired_content_height;
    
    // Window sized to match Spout aspect ratio
    
    // Initialize Spout sender
    spout_sender_ = std::make_unique<RivuletSpoutSender>();
    if (!spout_sender_->Initialize("Rivulet Output")) {
        std::cerr << "❌ Failed to initialize Spout sender" << std::endl;
        return false;
    }
    
    // Create main window
    if (!CreateMainWindow(config)) {
        std::cerr << "❌ Failed to create main window" << std::endl;
        return false;
    }
    
    // Initialize DirectX 11 for hardware acceleration
    if (InitializeDirectX11()) {
        hardware_acceleration_enabled_ = true;
        std::cout << "✅ DirectX 11 hardware acceleration enabled" << std::endl;
    } else {
        hardware_acceleration_enabled_ = false;
        std::cout << "⚠️ DirectX 11 initialization failed, using CPU fallback" << std::endl;
    }
    
    // Create client - this will create the browser after window is shown
    client_ = new BrowserClient(this);
    
    initialized_ = true;
    std::cout << "✅ Rivulet Browser Window initialized successfully" << std::endl;
    return true;
}

bool RivuletBrowserWindow::CreateMainWindow(const Config& config) {
    // Register window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = instance_;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = window_class_.c_str();
    
    if (!RegisterClassExW(&wcex)) {
        std::cerr << "Failed to register window class" << std::endl;
        return false;
    }
    
    // Create main window
    hwnd_ = CreateWindowW(
        window_class_.c_str(),
        L"Rivulet - CEF-Spout Video Sharing",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_width_, window_height_ + TOOLBAR_HEIGHT,
        nullptr, nullptr, instance_, this
    );
    
    if (!hwnd_) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }
    
    return true;
}

void RivuletBrowserWindow::CreateControls() {
    // Create font for controls
    font_ = CreateFont(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"MS Sans Serif"
    );
    
    int x = TOOLBAR_PADDING;
    int y = TOOLBAR_PADDING;
    
    // Back button
    back_hwnd_ = CreateWindowW(
        L"BUTTON", L"Back",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_BACK)), instance_, nullptr
    );
    SendMessage(back_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    x += BUTTON_WIDTH + TOOLBAR_PADDING;
    
    // Forward button
    forward_hwnd_ = CreateWindowW(
        L"BUTTON", L"Forward",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_FORWARD)), instance_, nullptr
    );
    SendMessage(forward_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    x += BUTTON_WIDTH + TOOLBAR_PADDING;
    
    // Reload button
    reload_hwnd_ = CreateWindowW(
        L"BUTTON", L"Reload",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_RELOAD)), instance_, nullptr
    );
    SendMessage(reload_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    x += BUTTON_WIDTH + TOOLBAR_PADDING;
    
    // Stop button
    stop_hwnd_ = CreateWindowW(
        L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_STOP)), instance_, nullptr
    );
    SendMessage(stop_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    x += BUTTON_WIDTH + TOOLBAR_PADDING;
    
    // Resolution dropdown (editable for custom resolutions)
    resolution_hwnd_ = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL,
        x, y, RESOLUTION_WIDTH, 200, // Height includes dropdown area
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_RESOLUTION)), instance_, nullptr
    );
    SendMessage(resolution_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    
    // Populate resolution dropdown with common resolutions
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1920x1080"));
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1280x720"));
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1024x768"));
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"800x600"));
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3840x2160"));
    SendMessage(resolution_hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2500x2500"));
    
    // Set current resolution as selected (may be from loaded settings)
    wchar_t current_res[32];
    swprintf_s(current_res, L"%dx%d", spout_width_, spout_height_);
    int current_index = static_cast<int>(SendMessage(resolution_hwnd_, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(current_res)));
    if (current_index != CB_ERR) {
        SendMessage(resolution_hwnd_, CB_SETCURSEL, current_index, 0);
    } else {
        // Custom resolution not in dropdown - set as text directly
        SetWindowTextW(resolution_hwnd_, current_res);
    }
    
    x += RESOLUTION_WIDTH + TOOLBAR_PADDING;
    
    // URL edit box - leave space for Go button and resolution dropdown
    int edit_width = window_width_ - x - TOOLBAR_PADDING - BUTTON_WIDTH - TOOLBAR_PADDING;
    edit_hwnd_ = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
        x, y, edit_width, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_URL_EDIT)), instance_, nullptr
    );
    SendMessage(edit_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    x += edit_width + TOOLBAR_PADDING;
    
    // Go button
    go_hwnd_ = CreateWindowW(
        L"BUTTON", L"Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_GO)), instance_, nullptr
    );
    SendMessage(go_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    
    // Subclass the edit control to handle Enter key
    edit_wndproc_old_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(edit_hwnd_, GWLP_WNDPROC, 
                                                                  reinterpret_cast<LONG_PTR>(EditProc)));
    SetWindowLongPtr(edit_hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    // Subclass the resolution combobox edit control to handle Enter key
    HWND resolution_edit = FindWindowEx(resolution_hwnd_, nullptr, L"EDIT", nullptr);
    if (resolution_edit) {
        resolution_wndproc_old_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(resolution_edit, GWLP_WNDPROC,
                                                                           reinterpret_cast<LONG_PTR>(ResolutionProc)));
        SetWindowLongPtr(resolution_edit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    
    std::cout << "✅ Browser controls created" << std::endl;
}

int RivuletBrowserWindow::RunMessageLoop() {
    if (!initialized_) {
        std::cerr << "❌ Browser window not initialized" << std::endl;
        return -1;
    }
    
    std::cout << "🎬 Starting Rivulet message loop..." << std::endl;
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    
    if (use_synchronized_rendering_ && hardware_acceleration_enabled_) {
        return RunSynchronizedRenderLoop();
    } else {
        return RunLegacyMessageLoop();
    }
}

int RivuletBrowserWindow::RunSynchronizedRenderLoop() {
    std::cout << "🚀 Starting V-Sync synchronized render loop for perfect frame timing" << std::endl;
    std::cout << "🎯 Present(1, 0) will provide hardware-perfect pacing - no manual Sleep needed" << std::endl;
    
    MSG msg;
    DWORD frame_start_time = GetTickCount();
    int frame_count = 0;
    bool has_rendered_first_frame = false;
    
    while (!is_closing_) {
        // Handle Windows messages (non-blocking)
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                is_closing_ = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (is_closing_) break;
        
        // Let CEF do its work (non-blocking)
        CefDoMessageLoopWork();
        
        // Request a new frame from CEF if browser is ready
        if (browser_) {
            // Tell CEF we're ready for a new frame
            browser_->GetHost()->SendExternalBeginFrame();
            
            // Wait for a new frame, but only if we don't have one already
            // Use a longer timeout for the first frame, shorter for subsequent frames
            DWORD timeout = has_rendered_first_frame ? 33 : 100; // 33ms = 2 V-Sync periods, 100ms for first frame
            DWORD wait_result = WaitForSingleObject(frame_ready_event_, timeout);
            
            if (wait_result == WAIT_OBJECT_0) {
                // New frame is ready! Render it immediately
                if (hardware_acceleration_enabled_ && shared_texture_) {
                    RenderDirectXFrame(); // Present(1, 0) inside here blocks until V-Sync
                    has_rendered_first_frame = true;
                }
                
                // Reset frame ready flag
                new_frame_ready_ = false;
                frame_count++;
                
                // Print FPS every 3 seconds (less spam)
                DWORD current_time = GetTickCount();
                if (current_time - frame_start_time >= 3000) {
                    double fps = (double)frame_count * 1000.0 / (current_time - frame_start_time);
                    std::cout << "📊 V-Sync locked at " << fps << " FPS (hardware-perfect timing)" << std::endl;
                    frame_count = 0;
                    frame_start_time = current_time;
                }
            } else if (wait_result == WAIT_TIMEOUT) {
                // No new frame within timeout period
                if (has_rendered_first_frame && hardware_acceleration_enabled_ && shared_texture_) {
                    // Re-present the current frame to maintain smooth display
                    // Present(1, 0) will still sync to V-Blank for smooth animation
                    RenderDirectXFrame(); 
                } else {
                    // No frame available yet, minimal sleep to avoid 100% CPU
                    Sleep(1);
                }
            }
        } else {
            // Browser not ready yet, small sleep to avoid spinning
            Sleep(1);
        }
        
        // CRITICAL: No manual Sleep here! Present(1, 0) provides perfect V-Sync timing
        // The Present call inside RenderDirectXFrame blocks until the monitor is ready
    }
    
    std::cout << "✅ V-Sync synchronized render loop finished" << std::endl;
    return static_cast<int>(msg.wParam);
}

int RivuletBrowserWindow::RunLegacyMessageLoop() {
    std::cout << "⏰ Using legacy message loop" << std::endl;
    
    MSG msg;
    while (!is_closing_) {
        // Handle Windows messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                is_closing_ = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (is_closing_) break;
        
        // Let CEF do its work
        CefDoMessageLoopWork();
        
        // Small sleep to prevent 100% CPU usage
        Sleep(1);
    }
    
    return static_cast<int>(msg.wParam);
}

void RivuletBrowserWindow::Shutdown() {
    if (!initialized_) return;
    
    std::cout << "Shutting down Rivulet browser window..." << std::endl;
    
    if (browser_) {
        browser_->GetHost()->CloseBrowser(true);
        browser_ = nullptr;
    }
    
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    
    spout_sender_.reset();
    
    // Clean up rendering resources
    if (hardware_acceleration_enabled_) {
        ShutdownDirectX11();
    }
    DestroyOffScreenBitmap();
    
    initialized_ = false;
    std::cout << "✅ Rivulet browser window shutdown complete" << std::endl;
}

LRESULT CALLBACK RivuletBrowserWindow::EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RivuletBrowserWindow* window = reinterpret_cast<RivuletBrowserWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (window && message == WM_KEYDOWN && wParam == VK_RETURN) {
        // Enter key pressed in URL edit box - navigate to URL
        window->OnGo();
        return 0;
    }
    
    // Call original edit control procedure
    return CallWindowProc(window ? window->edit_wndproc_old_ : DefWindowProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK RivuletBrowserWindow::ResolutionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RivuletBrowserWindow* window = reinterpret_cast<RivuletBrowserWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (window && message == WM_KEYDOWN && wParam == VK_RETURN) {
        // Enter key pressed in resolution edit box - change resolution
        window->OnResolutionChange();
        return 0;
    }
    
    // Call original edit control procedure
    return CallWindowProc(window ? window->resolution_wndproc_old_ : DefWindowProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK RivuletBrowserWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RivuletBrowserWindow* window = nullptr;
    
    if (message == WM_NCCREATE) {
        LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        window = static_cast<RivuletBrowserWindow*>(pcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        
        // Set the hwnd_ member immediately
        window->hwnd_ = hwnd;
        // Window created successfully
    } else {
        window = reinterpret_cast<RivuletBrowserWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (window) {
        return window->HandleMessage(hwnd, message, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT RivuletBrowserWindow::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            OnCreate(reinterpret_cast<LPCREATESTRUCT>(lParam));
            return 0;
            
        case WM_SIZE:
            OnSize();
            return 0;
            
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_COMMAND:
            OnCommand(wParam);
            return 0;
            
        case WM_SETFOCUS:
            if (browser_) {
                browser_->GetHost()->SetFocus(true);
            }
            return 0;
            
        case WM_KILLFOCUS:
            if (browser_) {
                browser_->GetHost()->SetFocus(false);
            }
            return 0;
            
        // Mouse events for off-screen rendering
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
            OnMouseEvent(message, wParam, lParam);
            return 0;
            
        // Keyboard events for off-screen rendering
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
            // Only forward to CEF if focus is not on URL edit box
            if (GetFocus() != edit_hwnd_) {
                OnKeyEvent(message, wParam, lParam);
                return 0;
            }
            break;

    case WM_TIMER: {
        // Only handle timer when not using synchronized rendering
        if (!use_synchronized_rendering_) {
            // Only invalidate the CEF content area, not the entire window.
            // This prevents the toolbar and controls from flickering.
            int toolbar_height = toolbar_visible_ ? TOOLBAR_HEIGHT : 0;
            RECT cef_rect = {0, toolbar_height, window_width_, window_height_};
            InvalidateRect(hwnd_, &cef_rect, FALSE);
        }
        return 0;
    }
            
        case WM_ERASEBKGND:
            return 1; // Don't erase background
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_USER + 1: // Custom message to create CEF browser
            CreateCefBrowser();
            return 0;
            
        case WM_GETMINMAXINFO: {
            // Maintain aspect ratio during resize
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            float spout_aspect = (float)spout_width_ / spout_height_;
            
            // Set minimum size
            int min_content_height = 400;
            int min_content_width = (int)(min_content_height * spout_aspect);
            mmi->ptMinTrackSize.x = min_content_width;
            mmi->ptMinTrackSize.y = min_content_height + TOOLBAR_HEIGHT;
            
            // Set maximum size  
            int max_content_height = 1200;
            int max_content_width = (int)(max_content_height * spout_aspect);
            mmi->ptMaxTrackSize.x = max_content_width;
            mmi->ptMaxTrackSize.y = max_content_height + TOOLBAR_HEIGHT;
            
            return 0;
        }
        
        case WM_SIZING: {
            // Enforce aspect ratio during window resize
            RECT* rect = reinterpret_cast<RECT*>(lParam);
            float spout_aspect = (float)spout_width_ / spout_height_;
            
            int window_width = rect->right - rect->left;
            int window_height = rect->bottom - rect->top;
            int content_height = window_height - TOOLBAR_HEIGHT;
            int content_width = window_width;
            
            // Calculate what the width should be for proper aspect ratio
            int correct_content_width = (int)(content_height * spout_aspect);
            int correct_window_width = correct_content_width;
            
            // Adjust based on which edge is being dragged
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                    // Width is changing - adjust height
                    rect->bottom = rect->top + (int)(content_width / spout_aspect) + TOOLBAR_HEIGHT;
                    break;
                    
                default:
                    // Height is changing - adjust width  
                    rect->right = rect->left + correct_window_width;
                    break;
            }
            
            return TRUE;
        }
            
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

void RivuletBrowserWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    CreateControls();
    
    // Delay CEF browser creation until after window is fully ready
    // Post message to create CEF browser after window initialization
    PostMessage(hwnd_, WM_USER + 1, 0, 0); // Custom message to create browser

    // Initialize synchronized rendering instead of timer-based updates
    if (use_synchronized_rendering_) {
        std::cout << "🎯 Using synchronized render loop for perfect frame timing" << std::endl;
    } else {
        // Fallback to timer for legacy mode
        SetTimer(hwnd_, 1, 16, nullptr); // Timer ID 1, ~16ms interval
        std::cout << "⏰ Using legacy timer-based rendering" << std::endl;
    }
}

void RivuletBrowserWindow::CreateCefBrowser() {
    // Creating CEF browser
    
    // Validate window handle
    if (!hwnd_ || !IsWindow(hwnd_)) {
        std::cerr << "❌ Invalid window handle for CEF browser creation" << std::endl;
        return;
    }
    
    // Create CEF browser after window is created
    CefWindowInfo window_info;
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    
    // Setting up CEF browser configuration
    
    // Configure CEF for hardware-accelerated shared texture rendering
    if (hardware_acceleration_enabled_) {
        window_info.SetAsWindowless(hwnd_);
        window_info.shared_texture_enabled = true;
        std::cout << "🚀 CEF configured for shared texture rendering" << std::endl;
    } else {
        // Fallback to CPU-based off-screen rendering
        window_info.SetAsWindowless(hwnd_);
        std::cout << "⚠️ CEF using CPU fallback rendering" << std::endl;
    }
    
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;
    
    if (hardware_acceleration_enabled_) {
        // Enable hardware acceleration features
        browser_settings.webgl = STATE_ENABLED;
    }
    
    // Create browser with startup URL
    std::string startup_url = "https://www.google.com";
    
    // CEF browser will use off-screen rendering
    
    bool result = CefBrowserHost::CreateBrowser(window_info, client_, startup_url, browser_settings, nullptr, nullptr);
    
    if (!result) {
        std::cerr << "❌ Failed to create CEF browser - CreateBrowser returned false" << std::endl;
        std::cerr << "   This usually means CEF is not properly initialized or the window info is invalid" << std::endl;
    } else {
        // CEF browser creation initiated
    }
}

void RivuletBrowserWindow::OnSize() {
    RECT rect;
    GetClientRect(hwnd_, &rect);
    
    // NOTE: Do NOT call WasResized() for off-screen rendering!
    // CEF renders at fixed size (spout_width_ x spout_height_) regardless of window size.
    // Calling WasResized() creates a contradiction with GetViewRect() and causes flickering.
    
    // Resize URL edit box and Go button (accounting for resolution dropdown)
    if (edit_hwnd_ && go_hwnd_ && resolution_hwnd_) {
        // Calculate total fixed width: 4 nav buttons + resolution dropdown + go button + padding
        int total_fixed_width = BUTTON_WIDTH * 5 + RESOLUTION_WIDTH + TOOLBAR_PADDING * 8;
        int edit_width = (rect.right - rect.left) - total_fixed_width;
        
        // Position after: Back + Forward + Reload + Stop + Resolution
        int edit_x = BUTTON_WIDTH * 4 + RESOLUTION_WIDTH + TOOLBAR_PADDING * 6;
        
        SetWindowPos(edit_hwnd_, nullptr,
                    edit_x, TOOLBAR_PADDING,
                    edit_width, BUTTON_HEIGHT,
                    SWP_NOZORDER);
                    
        SetWindowPos(go_hwnd_, nullptr,
                    edit_x + edit_width + TOOLBAR_PADDING, TOOLBAR_PADDING,
                    BUTTON_WIDTH, BUTTON_HEIGHT,
                    SWP_NOZORDER);
    }
}

void RivuletBrowserWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // Draw toolbar background (only if visible)
    int toolbar_height = toolbar_visible_ ? TOOLBAR_HEIGHT : 0;
    if (toolbar_visible_) {
        RECT toolbar_rect = {0, 0, window_width_, TOOLBAR_HEIGHT};
        FillRect(hdc, &toolbar_rect, (HBRUSH)(COLOR_BTNFACE + 1));
    }
    
    if (hardware_acceleration_enabled_) {
        // Hardware-accelerated rendering using DirectX
        RenderDirectXFrame();
        
        // Fill content area with black (DirectX handles actual rendering)
        RECT content_rect = {0, toolbar_height, window_width_, window_height_};
        FillRect(hdc, &content_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
    } else {
        // Legacy CPU-based rendering
        // Lock bitmap access - UI thread reading
        EnterCriticalSection(&bitmap_lock_);
        
        if (off_screen_dc_ && off_screen_bitmap_ && bitmap_pixels_) {
        RECT content_rect;
        GetClientRect(hwnd_, &content_rect);
        content_rect.top = toolbar_height;
        
        // Calculate aspect ratio preserving dimensions
        int content_width = content_rect.right - content_rect.left;
        int content_height = content_rect.bottom - content_rect.top;
        
        float content_aspect = (float)content_width / content_height;
        float spout_aspect = (float)bitmap_width_ / bitmap_height_;
        
        int draw_width, draw_height, draw_x, draw_y;
        
        if (content_aspect > spout_aspect) {
            // Content is wider - fit to width, center vertically
            draw_width = content_width;
            draw_height = (int)(content_width / spout_aspect);
            draw_x = content_rect.left;
            draw_y = content_rect.top + (content_height - draw_height) / 2;
        } else {
            // Content is taller - fit to height, center horizontally
            draw_height = content_height;
            draw_width = (int)(content_height * spout_aspect);
            draw_x = content_rect.left + (content_width - draw_width) / 2;
            draw_y = content_rect.top;
        }
        
        // Use high-quality StretchBlt for direct DC-to-DC blitting (superior to StretchDIBits)
        SetStretchBltMode(hdc, HALFTONE); // High-quality stretch mode for smooth scaling
        SetBrushOrgEx(hdc, 0, 0, NULL);   // Set brush origin for proper dithering
        
        // Paint only the letterbox bars (not the entire content area) to avoid flicker
        HBRUSH black_brush = (HBRUSH)GetStockObject(BLACK_BRUSH);
        
        // Top letterbox bar (if any)
        if (draw_y > content_rect.top) {
            RECT top_bar = {content_rect.left, content_rect.top, content_rect.right, draw_y};
            FillRect(hdc, &top_bar, black_brush);
        }
        
        // Bottom letterbox bar (if any)
        if (draw_y + draw_height < content_rect.bottom) {
            RECT bottom_bar = {content_rect.left, draw_y + draw_height, content_rect.right, content_rect.bottom};
            FillRect(hdc, &bottom_bar, black_brush);
        }
        
        // Left letterbox bar (if any)
        if (draw_x > content_rect.left) {
            RECT left_bar = {content_rect.left, draw_y, draw_x, draw_y + draw_height};
            FillRect(hdc, &left_bar, black_brush);
        }
        
        // Right letterbox bar (if any)
        if (draw_x + draw_width < content_rect.right) {
            RECT right_bar = {draw_x + draw_width, draw_y, content_rect.right, draw_y + draw_height};
            FillRect(hdc, &right_bar, black_brush);
        }
        
        //Now, draw the CEF content on top of the prepared background
        BOOL result = StretchBlt(hdc, 
                               draw_x, draw_y, draw_width, draw_height,
                               off_screen_dc_, 
                               0, 0, bitmap_width_, bitmap_height_,
                               SRCCOPY);

        // Send frame to Spout from the UI thread to avoid blocking the CEF render thread.
        // This synchronizes the preview window with the Spout output.
        if (spout_sender_) {
            spout_sender_->SendFrame(static_cast<const uint8_t*>(bitmap_pixels_), bitmap_width_, bitmap_height_);
        }

            if (!result) {
                std::cerr << "❌ StretchBlt failed - Error: " << GetLastError() << std::endl;
            }
        } else {
            // Clear content area if no frame available
            RECT content_rect = {0, TOOLBAR_HEIGHT, window_width_, window_height_};
            FillRect(hdc, &content_rect, (HBRUSH)(COLOR_WINDOW + 1));
        }
        
        // Unlock bitmap access
        LeaveCriticalSection(&bitmap_lock_);
    }
    
    EndPaint(hwnd_, &ps);
}

void RivuletBrowserWindow::OnCommand(WPARAM wParam) {
    int id = LOWORD(wParam);
    
    switch (id) {
        case ID_BACK:
            OnBack();
            break;
        case ID_FORWARD:
            OnForward();
            break;
        case ID_RELOAD:
            OnReload();
            break;
        case ID_STOP:
            OnStop();
            break;
        case ID_URL_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                // URL changed
            }
            break;
        case ID_GO:
            OnGo();
            break;
        case ID_RESOLUTION:
            if (HIWORD(wParam) == CBN_CLOSEUP) {
                // Dropdown closed - check if selection actually changed
                OnResolutionChange();
            }
            break;
    }
}

void RivuletBrowserWindow::OnDestroy() {
    // Clean up the UI timer (if using legacy mode)
    if (!use_synchronized_rendering_) {
        KillTimer(hwnd_, 1);
    }
    
    // Save final settings before closing
    SaveSettings();

    is_closing_ = true;
    PostQuitMessage(0);
}

void RivuletBrowserWindow::OnBack() {
    if (browser_ && browser_->CanGoBack()) {
        browser_->GoBack();
    }
}

void RivuletBrowserWindow::OnForward() {
    if (browser_ && browser_->CanGoForward()) {
        browser_->GoForward();
    }
}

void RivuletBrowserWindow::OnReload() {
    if (browser_) {
        browser_->Reload();
    }
}

void RivuletBrowserWindow::OnStop() {
    if (browser_) {
        browser_->StopLoad();
    }
}

void RivuletBrowserWindow::OnGo() {
    if (!browser_ || !edit_hwnd_) return;
    
    wchar_t url_buffer[1024];
    int len = GetWindowTextW(edit_hwnd_, url_buffer, sizeof(url_buffer) / sizeof(wchar_t));
    
    if (len > 0) {
        std::wstring url(url_buffer);
        // Add protocol if needed
        if (url.find(L"://") == std::wstring::npos) {
            url = L"https://" + url;
        }
        
        browser_->GetMainFrame()->LoadURL(url);
        
        // Save the new URL to settings
        SaveSettings();
    }
}

void RivuletBrowserWindow::OnResolutionChange() {
    if (!resolution_hwnd_) return;
    
    // Get resolution text (handles both dropdown selection and custom input)
    wchar_t resolution_text[32];
    int text_length = GetWindowTextW(resolution_hwnd_, resolution_text, sizeof(resolution_text) / sizeof(wchar_t));
    
    if (text_length == 0) {
        std::cerr << "❌ No resolution text entered" << std::endl;
        return;
    }
    
    // Parse resolution (format: "1920x1080")
    int new_width, new_height;
    if (swscanf_s(resolution_text, L"%dx%d", &new_width, &new_height) != 2) {
        std::cerr << "❌ Invalid resolution format. Use format: 1920x1080" << std::endl;
        return;
    }
    
    // Validate resolution bounds (reasonable limits)
    if (new_width < 100 || new_width > 7680 || new_height < 100 || new_height > 4320) {
        std::cerr << "❌ Resolution out of bounds. Width: 100-7680, Height: 100-4320" << std::endl;
        return;
    }
    
    // Update Spout dimensions
    spout_width_ = new_width;
    spout_height_ = new_height;
    
    // Calculate new window size maintaining aspect ratio
    float spout_aspect = (float)spout_width_ / spout_height_;
    int desired_content_height = 600; // Base content height
    int calculated_content_width = (int)(desired_content_height * spout_aspect);
    
    // Update window dimensions
    window_width_ = calculated_content_width;
    window_height_ = desired_content_height;
    
    // Resize the window
    SetWindowPos(hwnd_, nullptr, 0, 0, 
                window_width_, window_height_ + TOOLBAR_HEIGHT,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Trigger layout update
    OnSize();
    
    // Auto-reload the page to adapt content to new viewport dimensions
    if (browser_) {
        browser_->Reload();
        std::cout << "🔄 Page reloaded for new resolution" << std::endl;
    }
    
    std::cout << "✅ Resolution changed to " << new_width << "x" << new_height << std::endl;
    std::cout << "   Window resized to " << window_width_ << "x" << (window_height_ + TOOLBAR_HEIGHT) << std::endl;
    
    // Save the new resolution to settings
    SaveSettings();
}

void RivuletBrowserWindow::ToggleToolbar() {
    toolbar_visible_ = !toolbar_visible_;
    
    // Show/hide all toolbar controls
    int show_cmd = toolbar_visible_ ? SW_SHOW : SW_HIDE;
    ShowWindow(back_hwnd_, show_cmd);
    ShowWindow(forward_hwnd_, show_cmd);
    ShowWindow(reload_hwnd_, show_cmd);
    ShowWindow(stop_hwnd_, show_cmd);
    ShowWindow(edit_hwnd_, show_cmd);
    ShowWindow(go_hwnd_, show_cmd);
    ShowWindow(resolution_hwnd_, show_cmd);
    
    // Adjust window size to account for toolbar visibility
    int toolbar_height = toolbar_visible_ ? TOOLBAR_HEIGHT : 0;
    
    RECT window_rect;
    GetWindowRect(hwnd_, &window_rect);
    int window_width = window_rect.right - window_rect.left;
    int current_content_height = (window_rect.bottom - window_rect.top) - (toolbar_visible_ ? 0 : TOOLBAR_HEIGHT);
    
    // Resize window
    SetWindowPos(hwnd_, nullptr, 0, 0,
                window_width, current_content_height + toolbar_height,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Invalidate to force repaint with new layout
    InvalidateRect(hwnd_, nullptr, TRUE);
    
    std::cout << (toolbar_visible_ ? "✅ Toolbar shown" : "🔲 Toolbar hidden") << " (F11 to toggle)" << std::endl;
}

void RivuletBrowserWindow::OnMouseEvent(UINT message, WPARAM wParam, LPARAM lParam) {
    if (!browser_) return;
    
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    
    // Adjust for toolbar - only send events in content area
    int toolbar_height = toolbar_visible_ ? TOOLBAR_HEIGHT : 0;
    if (y < toolbar_height) return;
    y -= toolbar_height;
    
    // Scale coordinates to Spout output size with aspect ratio correction
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int content_width = client_rect.right;
    int content_height = client_rect.bottom - toolbar_height;
    
    if (content_width > 0 && content_height > 0) {
        // Calculate the actual drawing area (same logic as OnPaint)
        float content_aspect = (float)content_width / content_height;
        float spout_aspect = (float)spout_width_ / spout_height_;
        
        int draw_width, draw_height, draw_x, draw_y;
        
        if (content_aspect > spout_aspect) {
            draw_width = content_width;
            draw_height = (int)(content_width / spout_aspect);
            draw_x = 0;
            draw_y = (content_height - draw_height) / 2;
        } else {
            draw_height = content_height;
            draw_width = (int)(content_height * spout_aspect);
            draw_x = (content_width - draw_width) / 2;
            draw_y = 0;
        }
        
        // Check if click is within the actual content area
        if (x >= draw_x && x < draw_x + draw_width && y >= draw_y && y < draw_y + draw_height) {
            // Scale coordinates relative to the drawn content area
            x = ((x - draw_x) * spout_width_) / draw_width;
            y = ((y - draw_y) * spout_height_) / draw_height;
        } else {
            // Click is in letterbox area - ignore
            return;
        }
    }
    
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    
    // Set modifiers
    mouse_event.modifiers = 0;
    if (wParam & MK_CONTROL) mouse_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
    if (wParam & MK_SHIFT) mouse_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_MENU) & 0x8000) mouse_event.modifiers |= EVENTFLAG_ALT_DOWN;
    
    switch (message) {
        case WM_LBUTTONDOWN:
            // Set focus to main window when clicking in content area
            SetFocus(hwnd_);
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, false, 1);
            break;
        case WM_LBUTTONUP:
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_LEFT, true, 1);
            break;
        case WM_RBUTTONDOWN:
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, false, 1);
            break;
        case WM_RBUTTONUP:
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, true, 1);
            break;
        case WM_MBUTTONDOWN:
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_MIDDLE, false, 1);
            break;
        case WM_MBUTTONUP:
            browser_->GetHost()->SendMouseClickEvent(mouse_event, MBT_MIDDLE, true, 1);
            break;
        case WM_MOUSEMOVE:
            browser_->GetHost()->SendMouseMoveEvent(mouse_event, false);
            break;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            browser_->GetHost()->SendMouseWheelEvent(mouse_event, 0, delta);
            break;
        }
    }
}

void RivuletBrowserWindow::OnKeyEvent(UINT message, WPARAM wParam, LPARAM lParam) {
    if (!browser_) return;
    
    // Handle hotkeys first (before sending to CEF)
    if (message == WM_KEYDOWN && wParam == VK_F11) {
        ToggleToolbar();
        return; // Don't send F11 to CEF
    }
    
    CefKeyEvent key_event;
    key_event.windows_key_code = static_cast<int>(wParam);
    key_event.native_key_code = static_cast<int>(lParam);
    key_event.is_system_key = (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP || message == WM_SYSCHAR);
    
    // Set modifiers
    key_event.modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) key_event.modifiers |= EVENTFLAG_CONTROL_DOWN;
    if (GetKeyState(VK_SHIFT) & 0x8000) key_event.modifiers |= EVENTFLAG_SHIFT_DOWN;
    if (GetKeyState(VK_MENU) & 0x8000) key_event.modifiers |= EVENTFLAG_ALT_DOWN;
    
    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            key_event.type = KEYEVENT_KEYDOWN;
            browser_->GetHost()->SendKeyEvent(key_event);
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            key_event.type = KEYEVENT_KEYUP;
            browser_->GetHost()->SendKeyEvent(key_event);
            break;
        case WM_CHAR:
        case WM_SYSCHAR:
            key_event.type = KEYEVENT_CHAR;
            browser_->GetHost()->SendKeyEvent(key_event);
            break;
    }
}

// DirectX 11 hardware acceleration implementation
bool RivuletBrowserWindow::InitializeDirectX11() {
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.BufferDesc.Width = spout_width_;
    swap_chain_desc.BufferDesc.Height = spout_height_;
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.OutputWindow = hwnd_;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.Windowed = TRUE;
    
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,  // Remove in release
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &swap_chain_desc,
        &swap_chain_,
        &d3d11_device_,
        nullptr,
        &d3d11_context_
    );
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create DirectX 11 device and swap chain: " << std::hex << hr << std::endl;
        return false;
    }
    
    if (!CreateDirectXRenderTarget()) {
        std::cerr << "❌ Failed to create DirectX render target" << std::endl;
        return false;
    }
    
    if (!CreateTextureRenderingPipeline()) {
        std::cerr << "❌ Failed to create texture rendering pipeline" << std::endl;
        return false;
    }
    
    std::cout << "✅ DirectX 11 device and rendering pipeline initialized successfully" << std::endl;
    return true;
}

void RivuletBrowserWindow::ShutdownDirectX11() {
    render_target_view_.Reset();
    shared_texture_.Reset();
    swap_chain_.Reset();
    d3d11_context_.Reset();
    d3d11_device_.Reset();
    std::cout << "✅ DirectX 11 resources cleaned up" << std::endl;
}

bool RivuletBrowserWindow::CreateDirectXRenderTarget() {
    ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to get swap chain back buffer: " << std::hex << hr << std::endl;
        return false;
    }
    
    hr = d3d11_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create render target view: " << std::hex << hr << std::endl;
        return false;
    }
    
    return true;
}

bool RivuletBrowserWindow::CreateTextureRenderingPipeline() {
    // Simple vertex shader for a fullscreen quad
    const char* vertex_shader_source = R"(
        struct VSInput {
            float2 position : POSITION;
            float2 texcoord : TEXCOORD0;
        };
        
        struct VSOutput {
            float4 position : SV_POSITION;
            float2 texcoord : TEXCOORD0;
        };
        
        VSOutput main(VSInput input) {
            VSOutput output;
            output.position = float4(input.position, 0.0f, 1.0f);
            output.texcoord = input.texcoord;
            return output;
        }
    )";
    
    // Simple pixel shader to sample the texture
    const char* pixel_shader_source = R"(
        Texture2D shaderTexture : register(t0);
        SamplerState samplerState : register(s0);
        
        struct PSInput {
            float4 position : SV_POSITION;
            float2 texcoord : TEXCOORD0;
        };
        
        float4 main(PSInput input) : SV_TARGET {
            return shaderTexture.Sample(samplerState, input.texcoord);
        }
    )";
    
    // Compile vertex shader
    ComPtr<ID3DBlob> vs_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompile(
        vertex_shader_source,
        strlen(vertex_shader_source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        &vs_blob,
        &error_blob
    );
    
    if (FAILED(hr)) {
        if (error_blob) {
            std::cerr << "❌ Vertex shader compile error: " << (char*)error_blob->GetBufferPointer() << std::endl;
        }
        return false;
    }
    
    hr = d3d11_device_->CreateVertexShader(
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        nullptr,
        &vertex_shader_
    );
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create vertex shader: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Compile pixel shader
    ComPtr<ID3DBlob> ps_blob;
    hr = D3DCompile(
        pixel_shader_source,
        strlen(pixel_shader_source),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        &ps_blob,
        &error_blob
    );
    
    if (FAILED(hr)) {
        if (error_blob) {
            std::cerr << "❌ Pixel shader compile error: " << (char*)error_blob->GetBufferPointer() << std::endl;
        }
        return false;
    }
    
    hr = d3d11_device_->CreatePixelShader(
        ps_blob->GetBufferPointer(),
        ps_blob->GetBufferSize(),
        nullptr,
        &pixel_shader_
    );
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create pixel shader: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    
    hr = d3d11_device_->CreateInputLayout(
        layout,
        ARRAYSIZE(layout),
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        &input_layout_
    );
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create input layout: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Create vertex buffer for fullscreen quad
    struct Vertex {
        float position[2];
        float texcoord[2];
    };
    
    Vertex vertices[] = {
        // Triangle 1
        {{-1.0f, -1.0f}, {0.0f, 1.0f}}, // Bottom-left
        {{-1.0f,  1.0f}, {0.0f, 0.0f}}, // Top-left  
        {{ 1.0f, -1.0f}, {1.0f, 1.0f}}, // Bottom-right
        
        // Triangle 2
        {{ 1.0f, -1.0f}, {1.0f, 1.0f}}, // Bottom-right
        {{-1.0f,  1.0f}, {0.0f, 0.0f}}, // Top-left
        {{ 1.0f,  1.0f}, {1.0f, 0.0f}}  // Top-right
    };
    
    D3D11_BUFFER_DESC buffer_desc = {};
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.ByteWidth = sizeof(vertices);
    buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = vertices;
    
    hr = d3d11_device_->CreateBuffer(&buffer_desc, &init_data, &vertex_buffer_);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create vertex buffer: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Create sampler state
    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    
    hr = d3d11_device_->CreateSamplerState(&sampler_desc, &sampler_state_);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create sampler state: " << std::hex << hr << std::endl;
        return false;
    }
    
    std::cout << "✅ Texture rendering pipeline created successfully" << std::endl;
    return true;
}

void RivuletBrowserWindow::RenderDirectXFrame() {
    if (!d3d11_context_ || !render_target_view_) return;
    
    // Set render target
    d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);
    
    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = (float)spout_width_;
    viewport.Height = (float)spout_height_;  
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d11_context_->RSSetViewports(1, &viewport);
    
    // Clear the render target
    float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3d11_context_->ClearRenderTargetView(render_target_view_.Get(), clear_color);
    
    // If we have a shared texture from CEF, render it
    if (shared_texture_ && texture_srv_) {
        RenderTexturedQuad();
    }
    
    // Present the frame (1 = V-Sync enabled)
    swap_chain_->Present(1, 0);
}

void RivuletBrowserWindow::RenderTexturedQuad() {
    if (!d3d11_context_ || !vertex_shader_ || !pixel_shader_ || !texture_srv_) return;
    
    // Set shaders
    d3d11_context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    d3d11_context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
    
    // Set input layout
    d3d11_context_->IASetInputLayout(input_layout_.Get());
    
    // Set vertex buffer
    UINT stride = sizeof(float) * 4; // position(2) + texcoord(2)
    UINT offset = 0;
    d3d11_context_->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);
    
    // Set primitive topology
    d3d11_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Set texture and sampler
    d3d11_context_->PSSetShaderResources(0, 1, texture_srv_.GetAddressOf());
    d3d11_context_->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());
    
    // Draw the quad (6 vertices = 2 triangles)
    d3d11_context_->Draw(6, 0);
}

void RivuletBrowserWindow::OnSharedTextureUpdate(HANDLE shared_handle) {
    if (!shared_handle) {
        std::cerr << "❌ Invalid shared handle received" << std::endl;
        return;
    }
    
    // Try multiple approaches to open the shared texture
    
    // Approach 1: Try with our D3D11 device
    if (d3d11_device_) {
        HRESULT hr = d3d11_device_->OpenSharedResource(
            shared_handle,
            IID_PPV_ARGS(&shared_texture_)
        );
        
        if (SUCCEEDED(hr)) {
            std::cout << "✅ Shared texture opened with our D3D11 device" << std::endl;
            
            // Get texture description to extract dimensions
            D3D11_TEXTURE2D_DESC desc;
            shared_texture_->GetDesc(&desc);
            
            std::cout << "📐 Texture dimensions: " << desc.Width << "x" << desc.Height << std::endl;
            std::cout << "🎨 Texture format: " << desc.Format << std::endl;
            
            // Create shader resource view for rendering
            texture_srv_.Reset(); // Release previous SRV
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = desc.Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            srv_desc.Texture2D.MostDetailedMip = 0;
            
            HRESULT srv_hr = d3d11_device_->CreateShaderResourceView(
                shared_texture_.Get(),
                &srv_desc,
                &texture_srv_
            );
            
            if (FAILED(srv_hr)) {
                std::cerr << "❌ Failed to create shader resource view: " << std::hex << srv_hr << std::endl;
                return;
            }
            
            // Send texture directly to Spout for zero-copy sharing!
            if (spout_sender_) {
                bool success = spout_sender_->SendTexture(shared_texture_.Get(), desc.Width, desc.Height);
                if (success) {
                    std::cout << "✅ Hardware-accelerated frame sent to Spout (zero-copy)" << std::endl;
                } else {
                    std::cerr << "❌ Failed to send texture to Spout" << std::endl;
                }
            }
            
            // Signal that a new frame is ready for synchronized rendering
            if (use_synchronized_rendering_) {
                new_frame_ready_ = true;
                SetEvent(frame_ready_event_);
            } else {
                // Legacy: trigger window redraw
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else {
            std::cerr << "❌ Failed to open shared texture with our device: " << std::hex << hr << std::endl;
        }
    }
    
    // Approach 2: Try opening with a new device created specifically for shared resources
    ComPtr<ID3D11Device> shared_device;
    ComPtr<ID3D11DeviceContext> shared_context;
    
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0, // No debug flag for shared device
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &shared_device,
        nullptr,
        &shared_context
    );
    
    if (SUCCEEDED(hr)) {
        ComPtr<ID3D11Texture2D> temp_texture;
        hr = shared_device->OpenSharedResource(
            shared_handle,
            IID_PPV_ARGS(&temp_texture)
        );
        
        if (SUCCEEDED(hr)) {
            std::cout << "✅ Shared texture opened with separate device" << std::endl;
            
            // Get texture description
            D3D11_TEXTURE2D_DESC desc;
            temp_texture->GetDesc(&desc);
            
            std::cout << "📐 Texture dimensions: " << desc.Width << "x" << desc.Height << std::endl;
            std::cout << "🎨 Texture format: " << desc.Format << std::endl;
            
            // TODO: Copy texture to our device or use it directly with Spout
            // For now, store it for the next frame
            shared_texture_ = temp_texture;
            
            // Trigger window redraw
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else {
            std::cerr << "❌ Failed to open shared texture with separate device: " << std::hex << hr << std::endl;
        }
    }
    
    std::cerr << "❌ All approaches to open shared texture failed" << std::endl;
}

// Legacy CPU fallback implementation
bool RivuletBrowserWindow::CreateOffScreenBitmap(int width, int height) {
    // Clean up existing bitmap if dimensions changed
    if (off_screen_bitmap_ && (bitmap_width_ != width || bitmap_height_ != height)) {
        DestroyOffScreenBitmap();
    }
    
    // Return early if bitmap already exists with correct dimensions
    if (off_screen_bitmap_ && bitmap_width_ == width && bitmap_height_ == height) {
        return true;
    }
    
    // Get main window DC for compatibility
    HDC main_dc = GetDC(hwnd_);
    if (!main_dc) {
        std::cerr << "❌ Failed to get main window DC" << std::endl;
        return false;
    }
    
    
    // Create compatible DC for off-screen drawing
    off_screen_dc_ = CreateCompatibleDC(main_dc);
    if (!off_screen_dc_) {
        std::cerr << "❌ Failed to create compatible DC" << std::endl;
        ReleaseDC(hwnd_, main_dc);
        return false;
    }
    
    // Create DIB section with explicit RGB format
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Negative for top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 32-bit ARGB format
    bmi.bmiHeader.biCompression = BI_RGB; // Standard RGB
    bmi.bmiHeader.biSizeImage = 0; // Can be 0 for BI_RGB
    
    off_screen_bitmap_ = CreateDIBSection(off_screen_dc_, &bmi, DIB_RGB_COLORS, &bitmap_pixels_, nullptr, 0);
    if (!off_screen_bitmap_ || !bitmap_pixels_) {
        DWORD error = GetLastError();
        std::cerr << "❌ Failed to create DIB section - Error: " << error << std::endl;
        DeleteDC(off_screen_dc_);
        off_screen_dc_ = nullptr;
        ReleaseDC(hwnd_, main_dc);
        return false;
    }
    
    
    // Select bitmap into DC
    old_bitmap_ = static_cast<HBITMAP>(SelectObject(off_screen_dc_, off_screen_bitmap_));
    
    // Store dimensions
    bitmap_width_ = width;
    bitmap_height_ = height;
    
    ReleaseDC(hwnd_, main_dc);
    
    return true;
}

void RivuletBrowserWindow::DestroyOffScreenBitmap() {
    // Lock to prevent race conditions during resource destruction
    EnterCriticalSection(&bitmap_lock_);

    if (off_screen_dc_) {
        // Restore old bitmap before cleanup
        if (old_bitmap_) {
            SelectObject(off_screen_dc_, old_bitmap_);
            old_bitmap_ = nullptr;
        }
        DeleteDC(off_screen_dc_);
        off_screen_dc_ = nullptr;
    }
    
    if (off_screen_bitmap_) {
        DeleteObject(off_screen_bitmap_);
        off_screen_bitmap_ = nullptr;
    }
    
    bitmap_pixels_ = nullptr;
    bitmap_width_ = 0;
    bitmap_height_ = 0;

    LeaveCriticalSection(&bitmap_lock_);
}

void RivuletBrowserWindow::UpdateOffScreenBitmap(const void* cef_buffer, int width, int height) {
    if (!cef_buffer) return;
    
    // Lock bitmap access - CEF render thread writing
    EnterCriticalSection(&bitmap_lock_);
    
    // Create off-screen bitmap if needed
    if (!CreateOffScreenBitmap(width, height)) {
        LeaveCriticalSection(&bitmap_lock_);
        return;
    }
    
    // Convert from CEF's BGRA to Windows DIB RGB format
    const uint8_t* src = static_cast<const uint8_t*>(cef_buffer);
    uint8_t* dst = static_cast<uint8_t*>(bitmap_pixels_);
    
    // Use memcpy for a highly efficient, direct memory copy.
    // The manual pixel-by-pixel loop is extremely slow and causes lock contention.
    memcpy(dst, src, width * height * 4);
    
    // Unlock bitmap access
    LeaveCriticalSection(&bitmap_lock_);
}

// Settings persistence implementation
std::wstring RivuletBrowserWindow::GetSettingsPath() {
    wchar_t documents[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents) == S_OK) {
        return std::wstring(documents) + L"\\rivulet_settings.txt";
    }
    // Fallback to current directory
    return L"rivulet_settings.txt";
}

void RivuletBrowserWindow::LoadSettings() {
    std::wstring settings_path = GetSettingsPath();
    std::wifstream file(settings_path);
    
    if (!file.is_open()) {
        std::wcout << L"No settings file found, using defaults" << std::endl;
        return;
    }
    
    std::wstring line;
    while (std::getline(file, line)) {
        if (line.find(L"resolution=") == 0) {
            std::wstring resolution = line.substr(11); // Skip "resolution="
            int width, height;
            if (swscanf_s(resolution.c_str(), L"%dx%d", &width, &height) == 2) {
                if (width >= 100 && width <= 7680 && height >= 100 && height <= 4320) {
                    spout_width_ = width;
                    spout_height_ = height;
                    std::wcout << L"✅ Loaded resolution: " << width << L"x" << height << std::endl;
                }
            }
        } else if (line.find(L"url=") == 0) {
            std::wstring url = line.substr(4); // Skip "url="
            if (!url.empty() && edit_hwnd_) {
                SetWindowTextW(edit_hwnd_, url.c_str());
                std::wcout << L"✅ Loaded URL: " << url << std::endl;
            }
        }
    }
    file.close();
}

void RivuletBrowserWindow::SaveSettings() {
    std::wstring settings_path = GetSettingsPath();
    std::wofstream file(settings_path);
    
    if (!file.is_open()) {
        std::wcerr << L"❌ Failed to save settings to: " << settings_path << std::endl;
        return;
    }
    
    // Save current resolution
    file << L"resolution=" << spout_width_ << L"x" << spout_height_ << std::endl;
    
    // Save current URL
    if (edit_hwnd_) {
        wchar_t url_buffer[1024];
        int len = GetWindowTextW(edit_hwnd_, url_buffer, sizeof(url_buffer) / sizeof(wchar_t));
        if (len > 0) {
            file << L"url=" << url_buffer << std::endl;
        }
    }
    
    file.close();
    std::wcout << L"✅ Settings saved to: " << settings_path << std::endl;
}

// BrowserClient implementation
void RivuletBrowserWindow::BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    parent_->browser_ = browser;
    
    std::cout << "✅ CEF browser created successfully (off-screen mode)" << std::endl;
    
    // For off-screen rendering, no browser window exists
    // All rendering goes through OnPaint callback
}

bool RivuletBrowserWindow::BrowserClient::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    return false;
}

void RivuletBrowserWindow::BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    parent_->browser_ = nullptr;
    // CEF browser closed
}

bool RivuletBrowserWindow::BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                                       CefRefPtr<CefFrame> frame,
                                                       int popup_id, 
                                                       const CefString& target_url,
                                                       const CefString& target_frame_name,
                                                       WindowOpenDisposition target_disposition,
                                                       bool user_gesture,
                                                       const CefPopupFeatures& popupFeatures,
                                                       CefWindowInfo& windowInfo,
                                                       CefRefPtr<CefClient>& client,
                                                       CefBrowserSettings& settings,
                                                       CefRefPtr<CefDictionaryValue>& extra_info,
                                                       bool* no_javascript_access) {
    CEF_REQUIRE_UI_THREAD();
    
    // Block all popups and redirect to main window
    std::cout << "🚫 Popup blocked, redirecting to main window: " << target_url.ToString() << std::endl;
    
    // Load the popup URL in the main browser instead
    if (browser && browser->GetMainFrame()) {
        browser->GetMainFrame()->LoadURL(target_url);
    }
    
    // Return true to block the popup
    return true;
}

void RivuletBrowserWindow::BrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                                              bool isLoading,
                                                              bool canGoBack,
                                                              bool canGoForward) {
    CEF_REQUIRE_UI_THREAD();
    
    // Update navigation buttons
    EnableWindow(parent_->back_hwnd_, canGoBack ? TRUE : FALSE);
    EnableWindow(parent_->forward_hwnd_, canGoForward ? TRUE : FALSE);
}

void RivuletBrowserWindow::BrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefFrame> frame,
                                                   int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();
    
    if (frame->IsMain()) {
        // Page loaded successfully
    }
}

void RivuletBrowserWindow::BrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                                         CefRefPtr<CefFrame> frame,
                                                         const CefString& url) {
    CEF_REQUIRE_UI_THREAD();
    
    if (frame->IsMain() && parent_->edit_hwnd_) {
        std::wstring wurl = url;
        SetWindowTextW(parent_->edit_hwnd_, wurl.c_str());
    }
}

void RivuletBrowserWindow::BrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                                       const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    
    std::wstring window_title = L"Rivulet - " + std::wstring(title);
    SetWindowTextW(parent_->hwnd_, window_title.c_str());
}

void RivuletBrowserWindow::BrowserClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, parent_->spout_width_, parent_->spout_height_);
}

void RivuletBrowserWindow::BrowserClient::OnPaint(CefRefPtr<CefBrowser> browser,
                                                 PaintElementType type,
                                                 const RectList& dirtyRects,
                                                 const void* buffer,
                                                 int width,
                                                 int height) {
    if (type != PET_VIEW) return;
    
    // This method is only called when hardware acceleration is disabled
    // (CPU fallback mode)
    if (!parent_->hardware_acceleration_enabled_ && buffer) {
        // Update off-screen bitmap for display
        parent_->UpdateOffScreenBitmap(buffer, width, height);
        
        // DO NOT invalidate here. The UI thread's WM_TIMER is now responsible
        // for driving repaints at a stable rate, decoupling the threads.
    }
}

void RivuletBrowserWindow::BrowserClient::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                                           PaintElementType type,
                                                           const RectList& dirtyRects,
                                                           const CefAcceleratedPaintInfo& info) {
    if (type != PET_VIEW) return;
    
    // This method is called when hardware acceleration is enabled
    // (shared texture mode)
    if (parent_->hardware_acceleration_enabled_) {
        if (parent_->use_synchronized_rendering_) {
            // Synchronized mode: minimal logging to avoid spam
            static int frame_counter = 0;
            if (++frame_counter % 60 == 0) {
                std::cout << "🎯 " << frame_counter << " synchronized frames received" << std::endl;
            }
        } else {
            std::cout << "🚀 Received hardware-accelerated frame from CEF" << std::endl;
        }
        
        parent_->OnSharedTextureUpdate(info.shared_texture_handle);
    }
}

} // namespace Rivulet