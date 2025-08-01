// Rivulet - Modern CEF-Spout Video Sharing Application
// web_layer.cpp - CEF web content implementation

#include "web_layer.h"
#include "d3d11_device.h"

#include "include/cef_browser.h"
#include "include/wrapper/cef_helpers.h"

#include <iostream>

namespace Rivulet {

WebLayer::WebLayer(std::shared_ptr<D3D11Device> device)
    : d3d11_device_(device)
    , width_(1280)
    , height_(720)
    , has_new_frame_(false)
    , initialized_(false)
    , pending_focus_(false) {
}

WebLayer::~WebLayer() {
    Shutdown();
}

bool WebLayer::Initialize(const std::string& url, int width, int height) {
    if (initialized_) return true;
    
    std::cout << "Initializing web layer: " << url << " (" << width << "x" << height << ")" << std::endl;
    
    width_ = width;
    height_ = height;
    
    // Window info for off-screen rendering
    CefWindowInfo window_info;
    window_info.SetAsWindowless(nullptr);  // Off-screen rendering
    
    // Shared texture support disabled for compatibility
    // window_info.shared_texture_enabled = true;
    // window_info.external_begin_frame_enabled = true;
    
    // Browser settings
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;  // Target 60 FPS
    
    // Create browser
    bool result = CefBrowserHost::CreateBrowser(
        window_info,
        this,  // client
        url,
        browser_settings,
        nullptr,  // extra_info
        nullptr   // request_context
    );
    
    if (!result) {
        std::cerr << "❌ Failed to create CEF browser" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "✅ Web layer initialized" << std::endl;
    return true;
}

void WebLayer::Shutdown() {
    if (!initialized_) return;
    
    if (browser_) {
        browser_->GetHost()->CloseBrowser(true);
        browser_ = nullptr;
    }
    
    // Shared texture cleanup disabled for now
    // shared_texture_.Reset();
    // shared_handle_ = nullptr;
    
    initialized_ = false;
    std::cout << "Web layer shut down" << std::endl;
}

void WebLayer::LoadURL(const std::string& url) {
    if (browser_) {
        browser_->GetMainFrame()->LoadURL(url);
        std::cout << "Loading URL: " << url << std::endl;
    }
}

void WebLayer::Reload() {
    if (browser_) {
        browser_->Reload();
        std::cout << "Reloading browser" << std::endl;
    }
}

void WebLayer::SendMouseClickEvent(int x, int y, bool is_left_button, bool mouse_up) {
    if (!browser_) return;
    
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    
    CefBrowserHost::MouseButtonType button_type = 
        is_left_button ? MBT_LEFT : MBT_RIGHT;
    
    browser_->GetHost()->SendMouseClickEvent(mouse_event, button_type, mouse_up, 1);
}

void WebLayer::SendMouseMoveEvent(int x, int y) {
    if (!browser_) return;
    
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    
    browser_->GetHost()->SendMouseMoveEvent(mouse_event, false);
}

void WebLayer::SendMouseWheelEvent(int x, int y, int delta_x, int delta_y) {
    if (!browser_) return;
    
    CefMouseEvent mouse_event;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.modifiers = 0;
    
    browser_->GetHost()->SendMouseWheelEvent(mouse_event, delta_x, delta_y);
}

void WebLayer::SendKeyEvent(int windows_key_code, bool key_up, bool is_char) {
    if (!browser_) return;
    
    CefKeyEvent key_event;
    key_event.windows_key_code = windows_key_code;
    key_event.native_key_code = windows_key_code;
    key_event.is_system_key = false;
    key_event.character = is_char ? windows_key_code : 0;
    key_event.unmodified_character = is_char ? windows_key_code : 0;
    key_event.modifiers = 0;
    
    if (is_char) {
        key_event.type = KEYEVENT_CHAR;
    } else {
        key_event.type = key_up ? KEYEVENT_KEYUP : KEYEVENT_KEYDOWN;
    }
    
    browser_->GetHost()->SendKeyEvent(key_event);
}

void WebLayer::SendFocusEvent(bool has_focus) {
    if (!browser_) {
        // Store focus state for when browser is created
        pending_focus_ = has_focus;
        return;
    }
    
    browser_->GetHost()->SetFocus(has_focus);
}

void WebLayer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
    rect = CefRect(0, 0, width_, height_);
}

void WebLayer::OnPaint(CefRefPtr<CefBrowser> browser,
                      PaintElementType type,
                      const RectList& dirtyRects,
                      const void* buffer,
                      int width,
                      int height) {
    
    if (type != PET_VIEW) return; // Only handle main view, not popup
    
    // Resize buffer if needed
    size_t buffer_size = width * height * 4; // BGRA format
    if (bitmap_buffer_.size() != buffer_size) {
        bitmap_buffer_.resize(buffer_size);
        std::cout << "Resized bitmap buffer for " << width << "x" << height << std::endl;
    }
    
    // Copy the bitmap data
    if (buffer) {
        memcpy(bitmap_buffer_.data(), buffer, buffer_size);
        has_new_frame_ = true;
        
        // Log occasionally
        static int frame_count = 0;
        frame_count++;
        if (frame_count % 60 == 0) {
            std::cout << "CEF frame " << frame_count << " - " << width << "x" << height << std::endl;
        }
    }
}

// OnAcceleratedPaint removed - not available in this CEF version
// Will implement shared texture support when available

void WebLayer::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
    
    // Apply pending focus state if any
    if (pending_focus_) {
        browser_->GetHost()->SetFocus(true);
        pending_focus_ = false;
    }
    
    std::cout << "✅ CEF browser created successfully" << std::endl;
}

bool WebLayer::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    return false;
}

void WebLayer::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
    std::cout << "CEF browser closed" << std::endl;
}

void WebLayer::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();
    
    if (frame->IsMain()) {
        std::cout << "✅ Page loaded successfully: " << frame->GetURL().ToString() << std::endl;
    }
}

void WebLayer::OnLoadError(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          ErrorCode errorCode,
                          const CefString& errorText,
                          const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();
    
    if (errorCode == ERR_ABORTED) return;  // Ignore aborted loads
    
    std::cerr << "❌ Load error: " << errorText.ToString() 
              << " (Code: " << errorCode << ")" << std::endl;
}

} // namespace Rivulet