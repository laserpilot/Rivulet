// Rivulet - Modern CEF-Spout Video Sharing Application
// rivulet_browser_window.cpp - Professional browser window implementation
// Adapted from cefclient root_window_win.cc

#include "rivulet_browser_window.h"
#include "spout_sender.h"

#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"

#include <iostream>
#include <windowsx.h>

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
    , font_(nullptr)
    , initialized_(false)
    , is_closing_(false)
    , spout_width_(1024)
    , spout_height_(768)
    , off_screen_dc_(nullptr)
    , off_screen_bitmap_(nullptr)
    , old_bitmap_(nullptr)
    , bitmap_pixels_(nullptr)
    , bitmap_width_(0)
    , bitmap_height_(0) {
}

RivuletBrowserWindow::~RivuletBrowserWindow() {
    Shutdown();
}

bool RivuletBrowserWindow::Initialize(const Config& config) {
    if (initialized_) return true;
    
    std::cout << "🚀 Initializing Rivulet Browser Window..." << std::endl;
    
    spout_width_ = config.spout_width;
    spout_height_ = config.spout_height;
    
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
    
    // URL edit box - leave space for Go button
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
    
    // Clean up double buffering resources
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
    
    // Use off-screen rendering for Spout integration
    window_info.SetAsWindowless(hwnd_);
    
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;
    
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
    
    // For off-screen rendering, notify CEF of size change
    if (browser_) {
        browser_->GetHost()->WasResized();
    }
    
    // Resize URL edit box and Go button
    if (edit_hwnd_ && go_hwnd_) {
        int total_button_width = BUTTON_WIDTH * 5 + TOOLBAR_PADDING * 7; // 4 nav buttons + 1 go button + padding
        int edit_width = (rect.right - rect.left) - total_button_width;
        
        SetWindowPos(edit_hwnd_, nullptr,
                    BUTTON_WIDTH * 4 + TOOLBAR_PADDING * 5, TOOLBAR_PADDING,
                    edit_width, BUTTON_HEIGHT,
                    SWP_NOZORDER);
                    
        SetWindowPos(go_hwnd_, nullptr,
                    BUTTON_WIDTH * 4 + TOOLBAR_PADDING * 6 + edit_width, TOOLBAR_PADDING,
                    BUTTON_WIDTH, BUTTON_HEIGHT,
                    SWP_NOZORDER);
    }
}

void RivuletBrowserWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // Draw toolbar background
    RECT toolbar_rect = {0, 0, window_width_, TOOLBAR_HEIGHT};
    FillRect(hdc, &toolbar_rect, (HBRUSH)(COLOR_BTNFACE + 1));
    
    // Draw CEF content from off-screen bitmap
    if (off_screen_dc_ && off_screen_bitmap_ && bitmap_pixels_) {
        RECT content_rect;
        GetClientRect(hwnd_, &content_rect);
        content_rect.top = TOOLBAR_HEIGHT;
        
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
        
        // Fill background with black bars if needed
        FillRect(hdc, &content_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        // Configure BITMAPINFO correctly for CEF's 32-bit BGRA top-down buffer
        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bitmap_width_;
        bmi.bmiHeader.biHeight = -bitmap_height_; // Negative = top-down DIB (matches CEF)
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32; // 32 bits per pixel (BGRA)
        bmi.bmiHeader.biCompression = BI_RGB; // No compression
        
        // Set blend mode for proper alpha handling
        int old_mode = SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        
        // Use StretchDIBits with properly configured BITMAPINFO
        int result = StretchDIBits(hdc,
                                  draw_x, draw_y, draw_width, draw_height,
                                  0, 0, bitmap_width_, bitmap_height_,
                                  bitmap_pixels_, &bmi, DIB_RGB_COLORS, SRCCOPY);
                                  
        // Restore blend mode
        SetStretchBltMode(hdc, old_mode);
        
        if (result == GDI_ERROR) {
            std::cerr << "❌ StretchDIBits failed - Error: " << GetLastError() << std::endl;
        }
    } else {
        // Clear content area if no frame available
        RECT content_rect = {0, TOOLBAR_HEIGHT, window_width_, window_height_};
        FillRect(hdc, &content_rect, (HBRUSH)(COLOR_WINDOW + 1));
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
    }
}

void RivuletBrowserWindow::OnDestroy() {
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
    }
}

void RivuletBrowserWindow::OnMouseEvent(UINT message, WPARAM wParam, LPARAM lParam) {
    if (!browser_) return;
    
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    
    // Adjust for toolbar - only send events in content area
    if (y < TOOLBAR_HEIGHT) return;
    y -= TOOLBAR_HEIGHT;
    
    // Scale coordinates to Spout output size with aspect ratio correction
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int content_width = client_rect.right;
    int content_height = client_rect.bottom - TOOLBAR_HEIGHT;
    
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

// Double buffering implementation
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
}

void RivuletBrowserWindow::UpdateOffScreenBitmap(const void* cef_buffer, int width, int height) {
    if (!cef_buffer) return;
    
    // Create off-screen bitmap if needed
    if (!CreateOffScreenBitmap(width, height)) {
        return;
    }
    
    // Convert from CEF's BGRA to Windows DIB RGB format
    const uint8_t* src = static_cast<const uint8_t*>(cef_buffer);
    uint8_t* dst = static_cast<uint8_t*>(bitmap_pixels_);
    
    // Windows StretchDIBits with BI_RGB actually expects BGRA format - keep CEF format!
    size_t pixel_count = width * height;
    for (size_t i = 0; i < pixel_count; ++i) {
        // Keep CEF's BGRA format - Windows BI_RGB 32-bit expects this order
        dst[i * 4 + 0] = src[i * 4 + 0]; // Blue <- CEF Blue
        dst[i * 4 + 1] = src[i * 4 + 1]; // Green <- CEF Green  
        dst[i * 4 + 2] = src[i * 4 + 2]; // Red <- CEF Red
        dst[i * 4 + 3] = 255;            // Alpha - force fully opaque
    }
    
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
    
    if (buffer) {
        // Update off-screen bitmap for display
        parent_->UpdateOffScreenBitmap(buffer, width, height);
        
        // Throttle window updates to reduce flickering - not every frame needs window redraw
        static DWORD last_window_update = 0;
        DWORD current_time = GetTickCount();
        if (current_time - last_window_update > 33) { // ~30fps window updates max
            InvalidateRect(parent_->hwnd_, nullptr, FALSE);
            last_window_update = current_time;
        }
        
        // Send frame to Spout (maintains perfect 60fps quality)
        if (parent_->spout_sender_) {
            parent_->spout_sender_->SendFrame(static_cast<const uint8_t*>(buffer), width, height);
        }
        
        // Debug output occasionally
        static int frame_count = 0;
        frame_count++;
        if (frame_count % 60 == 0) {
            // Frame rendered and sent to Spout
        }
    }
}

} // namespace Rivulet