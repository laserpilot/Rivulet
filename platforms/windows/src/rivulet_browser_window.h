// Rivulet - Modern CEF-Spout Video Sharing Application
// rivulet_browser_window.h - Professional browser window based on CEF cefclient
// Adapted from cefclient root_window_win.h

#ifndef RIVULET_BROWSER_WINDOW_H_
#define RIVULET_BROWSER_WINDOW_H_
#pragma once

#include <windows.h>
#include <commdlg.h>
#include <memory>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "spout_sender.h"

namespace Rivulet {

// Main browser window with navigation controls and Spout integration
class RivuletBrowserWindow {
public:
    struct Config {
        std::string startup_url = "https://www.testufo.com";
        int window_width = 1024;
        int window_height = 768;
        int spout_width = 1024;
        int spout_height = 768;
        bool with_controls = true;
        std::string window_title = "Rivulet - CEF-Spout Video Sharing";
    };

    explicit RivuletBrowserWindow(HINSTANCE instance);
    ~RivuletBrowserWindow();

    bool Initialize(const Config& config);
    bool PreInitializeForLuid();
    bool CompleteInitialization(const Config& config);
    int RunMessageLoop();
    int RunSynchronizedRenderLoop();
    int RunLegacyMessageLoop();
    void Shutdown();

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
    HWND GetWindowHandle() const { return hwnd_; }
    
    // GPU adapter synchronization
    std::string GetSelectedAdapterLuidString() const;
    
    // Console control
    static void SetVerboseLogging(bool enabled) { verbose_logging_ = enabled; }
    static bool IsVerboseLogging() { return verbose_logging_; }
    static void ToggleConsoleWindow();

private:
    // Window creation and management
    bool CreateMainWindow(const Config& config);
    void CreateControls();
    void CreateDirectXChildWindow();
    void CreateCefBrowser(); // Delayed CEF browser creation
    void SetDWMScalingOptimizations();

    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ResolutionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Event handlers
    void OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSize();
    void OnPaint();
    void OnCommand(WPARAM wParam);
    void OnDestroy();

    // Navigation event handlers
    void OnBack();
    void OnForward();
    void OnReload();
    void OnStop();
    void OnGo();
    void OnResolutionChange();
    void ToggleToolbar();
    
    // Input event handlers for off-screen rendering
    void OnMouseEvent(UINT message, WPARAM wParam, LPARAM lParam);
    void OnKeyEvent(UINT message, WPARAM wParam, LPARAM lParam);
    
    // Hardware acceleration management
    bool InitializeDirectX11();
    bool EnumerateGPUAdapters(); // Only enumerate adapters, don't create device
    void ShutdownDirectX11();
    bool CreateDirectXRenderTarget();
    bool CreateTextureRenderingPipeline();
    void RenderDirectXFrame();
    void RenderTexturedQuad();
    void ResizeDirectXBuffers(int width, int height);
    
    
    // Legacy CPU fallback management
    bool CreateOffScreenBitmap(int width, int height);
    void DestroyOffScreenBitmap();
    void UpdateOffScreenBitmap(const void* cef_buffer, int width, int height);
    
    // Shared texture handling
    void OnSharedTextureUpdate(HANDLE shared_handle);
    
    // Settings persistence
    void LoadSettings();
    void SaveSettings();
    std::wstring GetSettingsPath();

    // CEF Client implementation
    class BrowserClient : public CefClient,
                         public CefLifeSpanHandler,
                         public CefLoadHandler,
                         public CefDisplayHandler,
                         public CefRenderHandler {
    public:
        explicit BrowserClient(RivuletBrowserWindow* parent) : parent_(parent) {}

        // CefClient methods
        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
        CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
        CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
        CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }

        // CefLifeSpanHandler methods
        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
        bool DoClose(CefRefPtr<CefBrowser> browser) override;
        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
        bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                          bool* no_javascript_access) override;

        // CefLoadHandler methods
        void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                bool isLoading,
                                bool canGoBack,
                                bool canGoForward) override;
        void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      int httpStatusCode) override;

        // CefDisplayHandler methods
        void OnAddressChange(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           const CefString& url) override;
        void OnTitleChange(CefRefPtr<CefBrowser> browser,
                          const CefString& title) override;

        // CefRenderHandler methods (for hardware-accelerated shared texture rendering)
        void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        void OnPaint(CefRefPtr<CefBrowser> browser,
                    PaintElementType type,
                    const RectList& dirtyRects,
                    const void* buffer,
                    int width,
                    int height) override;
        void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                               PaintElementType type,
                               const RectList& dirtyRects,
                               const CefAcceleratedPaintInfo& info) override;

        IMPLEMENT_REFCOUNTING(BrowserClient);

    private:
        RivuletBrowserWindow* parent_;
    };

    // Application instance
    HINSTANCE instance_;
    
    // Main window
    HWND hwnd_;
    std::wstring window_class_;
    int window_width_;
    int window_height_;
    
    // Controls
    HWND back_hwnd_;
    HWND forward_hwnd_;
    HWND reload_hwnd_;
    HWND stop_hwnd_;
    HWND edit_hwnd_;    // URL edit box
    HWND go_hwnd_;      // Go button
    HWND resolution_hwnd_; // Resolution dropdown
    HWND apply_resolution_hwnd_; // Apply resolution button
    HWND directx_child_hwnd_; // Dedicated child window for DirectX content
    WNDPROC edit_wndproc_old_;
    WNDPROC resolution_wndproc_old_;
    
    // Font for controls
    HFONT font_;
    
    // CEF browser
    CefRefPtr<CefBrowser> browser_;
    CefRefPtr<BrowserClient> client_;
    
    // Spout integration
    std::unique_ptr<RivuletSpoutSender> spout_sender_;
    
    // State
    bool initialized_;
    bool is_closing_;
    bool toolbar_visible_;
    
    // Spout output dimensions
    int spout_width_;
    int spout_height_;
    
    // DirectX 11 hardware acceleration
    ComPtr<ID3D11Device> d3d11_device_;
    ComPtr<ID3D11DeviceContext> d3d11_context_;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID3D11RenderTargetView> render_target_view_;
    ComPtr<ID3D11Texture2D> shared_texture_;
    
    // Texture rendering pipeline
    ComPtr<ID3D11VertexShader> vertex_shader_;
    ComPtr<ID3D11PixelShader> pixel_shader_;
    ComPtr<ID3D11InputLayout> input_layout_;
    ComPtr<ID3D11Buffer> vertex_buffer_;
    ComPtr<ID3D11SamplerState> sampler_state_;
    ComPtr<ID3D11ShaderResourceView> texture_srv_;
    
    // Frame synchronization
    volatile bool new_frame_ready_;
    HANDLE frame_ready_event_;
    
    // GPU adapter information for CEF synchronization
    LUID selected_adapter_luid_;
    ComPtr<IDXGIAdapter> selected_adapter_; // Store the actual adapter instance
    
    // Legacy CPU fallback (for compatibility)
    HDC off_screen_dc_;
    HBITMAP off_screen_bitmap_;
    HBITMAP old_bitmap_;
    void* bitmap_pixels_;
    int bitmap_width_;
    int bitmap_height_;
    CRITICAL_SECTION bitmap_lock_;
    
    // Rendering mode
    bool hardware_acceleration_enabled_;
    bool use_synchronized_rendering_;
    
    // Console and debugging control
    static bool verbose_logging_;

    // Control IDs
    static constexpr int ID_BACK = 1001;
    static constexpr int ID_FORWARD = 1002;
    static constexpr int ID_RELOAD = 1003;
    static constexpr int ID_STOP = 1004;
    static constexpr int ID_URL_EDIT = 1005;
    static constexpr int ID_GO = 1006;
    static constexpr int ID_RESOLUTION = 1007;
    static constexpr int ID_APPLY_RESOLUTION = 1008;

    // Control dimensions
    static constexpr int BUTTON_WIDTH = 50;
    static constexpr int BUTTON_HEIGHT = 25;
    static constexpr int RESOLUTION_WIDTH = 120;
    static constexpr int TOOLBAR_HEIGHT = 35;
    static constexpr int TOOLBAR_PADDING = 5;

    DISALLOW_COPY_AND_ASSIGN(RivuletBrowserWindow);
};

} // namespace Rivulet

#endif // RIVULET_BROWSER_WINDOW_H_