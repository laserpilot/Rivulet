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
    , edit_wndproc_old_(nullptr)
    , font_(nullptr)
    , initialized_(false)
    , is_closing_(false)
    , spout_width_(1024)
    , spout_height_(768)
    , has_new_frame_(false) {
}

RivuletBrowserWindow::~RivuletBrowserWindow() {
    Shutdown();
}

bool RivuletBrowserWindow::Initialize(const Config& config) {
    if (initialized_) return true;
    
    std::cout << "🚀 Initializing Rivulet Browser Window..." << std::endl;
    
    window_width_ = config.window_width;
    window_height_ = config.window_height;
    spout_width_ = config.spout_width;
    spout_height_ = config.spout_height;
    
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
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
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
    
    // URL edit box
    int edit_width = window_width_ - x - TOOLBAR_PADDING;
    edit_hwnd_ = CreateWindowW(
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
        x, y, edit_width, BUTTON_HEIGHT,
        hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_URL_EDIT)), instance_, nullptr
    );
    SendMessage(edit_hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    
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
    
    initialized_ = false;
    std::cout << "✅ Rivulet browser window shutdown complete" << std::endl;
}

LRESULT CALLBACK RivuletBrowserWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    RivuletBrowserWindow* window = nullptr;
    
    if (message == WM_NCCREATE) {
        LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        window = static_cast<RivuletBrowserWindow*>(pcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        
        // Set the hwnd_ member immediately
        window->hwnd_ = hwnd;
        std::cout << "Window created, HWND set to: " << std::hex << hwnd << std::dec << std::endl;
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
            OnKeyEvent(message, wParam, lParam);
            return 0;
            
        case WM_ERASEBKGND:
            return 1; // Don't erase background
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_USER + 1: // Custom message to create CEF browser
            CreateCefBrowser();
            return 0;
            
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

void RivuletBrowserWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    CreateControls();
    
    // Delay CEF browser creation until after window is fully ready
    std::cout << "Posting message to create CEF browser after window initialization..." << std::endl;
    PostMessage(hwnd_, WM_USER + 1, 0, 0); // Custom message to create browser
}

void RivuletBrowserWindow::CreateCefBrowser() {
    std::cout << "Creating CEF browser (delayed)..." << std::endl;
    
    // Validate window handle
    if (!hwnd_ || !IsWindow(hwnd_)) {
        std::cerr << "❌ Invalid window handle for CEF browser creation" << std::endl;
        return;
    }
    
    // Create CEF browser after window is created
    CefWindowInfo window_info;
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    
    std::cout << "  Parent HWND: " << std::hex << hwnd_ << std::dec << std::endl;
    std::cout << "  Client rect: " << client_rect.right << "x" << client_rect.bottom << std::endl;
    
    // Use off-screen rendering for Spout integration
    window_info.SetAsWindowless(hwnd_);
    
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;
    
    // Create browser with startup URL - use Google for real website test
    std::string startup_url = "https://www.google.com";
    
    std::cout << "  URL: " << startup_url << std::endl;
    std::cout << "  Windowless: " << (window_info.windowless_rendering_enabled ? "Yes" : "No") << std::endl;
    
    bool result = CefBrowserHost::CreateBrowser(window_info, client_, startup_url, browser_settings, nullptr, nullptr);
    
    if (!result) {
        std::cerr << "❌ Failed to create CEF browser - CreateBrowser returned false" << std::endl;
        std::cerr << "   This usually means CEF is not properly initialized or the window info is invalid" << std::endl;
    } else {
        std::cout << "✅ CEF browser creation initiated successfully" << std::endl;
    }
}

void RivuletBrowserWindow::OnSize() {
    RECT rect;
    GetClientRect(hwnd_, &rect);
    
    // For off-screen rendering, notify CEF of size change
    if (browser_) {
        browser_->GetHost()->WasResized();
    }
    
    // Resize URL edit box
    if (edit_hwnd_) {
        int edit_width = (rect.right - rect.left) - (BUTTON_WIDTH * 4 + TOOLBAR_PADDING * 6);
        SetWindowPos(edit_hwnd_, nullptr,
                    BUTTON_WIDTH * 4 + TOOLBAR_PADDING * 5, TOOLBAR_PADDING,
                    edit_width, BUTTON_HEIGHT,
                    SWP_NOZORDER);
    }
}

void RivuletBrowserWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // Draw toolbar background
    RECT toolbar_rect = {0, 0, window_width_, TOOLBAR_HEIGHT};
    FillRect(hdc, &toolbar_rect, (HBRUSH)(COLOR_BTNFACE + 1));
    
    // Draw CEF content from off-screen buffer
    if (has_new_frame_ && !display_buffer_.empty()) {
        RECT content_rect;
        GetClientRect(hwnd_, &content_rect);
        content_rect.top = TOOLBAR_HEIGHT;
        
        // Create bitmap from buffer data
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = spout_width_;
        bmi.bmiHeader.biHeight = -spout_height_; // Negative for top-down bitmap
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        // Draw the bitmap stretched to fit the content area
        StretchDIBits(hdc,
            content_rect.left, content_rect.top,
            content_rect.right - content_rect.left,
            content_rect.bottom - content_rect.top,
            0, 0, spout_width_, spout_height_,
            display_buffer_.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
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
    
    // Scale coordinates to Spout output size
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int content_height = client_rect.bottom - TOOLBAR_HEIGHT;
    int content_width = client_rect.right;
    
    if (content_width > 0 && content_height > 0) {
        x = (x * spout_width_) / content_width;
        y = (y * spout_height_) / content_height;
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

// BrowserClient implementation
void RivuletBrowserWindow::BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    parent_->browser_ = browser;
    
    std::cout << "✅ CEF browser created successfully (off-screen mode)" << std::endl;
    std::cout << "   Browser: " << std::hex << browser.get() << std::dec << std::endl;
    std::cout << "   Parent HWND: " << std::hex << parent_->hwnd_ << std::dec << std::endl;
    std::cout << "   Spout output: " << parent_->spout_width_ << "x" << parent_->spout_height_ << std::endl;
    
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
    std::cout << "CEF browser closed" << std::endl;
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
        std::cout << "✅ Page loaded: " << frame->GetURL().ToString() << std::endl;
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
    
    // Copy buffer for display in window
    if (buffer) {
        size_t buffer_size = width * height * 4; // BGRA format
        parent_->display_buffer_.resize(buffer_size);
        memcpy(parent_->display_buffer_.data(), buffer, buffer_size);
        parent_->has_new_frame_ = true;
        
        // Trigger window repaint
        InvalidateRect(parent_->hwnd_, nullptr, FALSE);
        
        // Send frame to Spout
        if (parent_->spout_sender_) {
            parent_->spout_sender_->SendFrame(static_cast<const uint8_t*>(buffer), width, height);
        }
        
        // Debug output occasionally
        static int frame_count = 0;
        frame_count++;
        if (frame_count % 60 == 0) {
            std::cout << "Frame " << frame_count << " - " << width << "x" << height << " rendered" << std::endl;
        }
    }
}

} // namespace Rivulet