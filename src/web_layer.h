// Rivulet - Modern CEF-Spout Video Sharing Application
// web_layer.h - CEF web content layer with shared texture support

#pragma once

#include <include/cef_client.h>
#include <include/cef_render_handler.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>

#include <d3d11.h>
#include <memory>
#include <string>

namespace Rivulet {

class D3D11Device;

class WebLayer : public CefClient,
                 public CefRenderHandler,
                 public CefLifeSpanHandler,
                 public CefLoadHandler {
public:
    explicit WebLayer(std::shared_ptr<D3D11Device> device);
    ~WebLayer();

    // Initialization
    bool Initialize(const std::string& url, int width, int height);
    void Shutdown();

    // Navigation
    void LoadURL(const std::string& url);
    void Reload();

    // CefClient methods
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    // CefRenderHandler methods
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    void OnPaint(CefRefPtr<CefBrowser> browser,
                 PaintElementType type,
                 const RectList& dirtyRects,
                 const void* buffer,
                 int width,
                 int height) override;
    
    // Modern CEF shared texture support
    void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                           PaintElementType type,
                           const RectList& dirtyRects,
                           void* shared_handle) override;

    // CefLifeSpanHandler methods  
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler methods
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override;

    // Accessors
    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
    ID3D11Texture2D* GetSharedTexture() const { return shared_texture_.Get(); }
    HANDLE GetSharedHandle() const { return shared_handle_; }

private:
    IMPLEMENT_REFCOUNTING(WebLayer);

    std::shared_ptr<D3D11Device> d3d11_device_;
    CefRefPtr<CefBrowser> browser_;
    
    // Rendering properties
    int width_;
    int height_;
    
    // Shared texture for zero-copy rendering
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture_;
    HANDLE shared_handle_;
    
    bool initialized_;
};

} // namespace Rivulet