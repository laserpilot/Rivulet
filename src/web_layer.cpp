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
    , initialized_(false) {
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