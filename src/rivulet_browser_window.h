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

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/wrapper/cef_helpers.h"
#include "spout_sender.h"

namespace Rivulet {

// Main browser window with navigation controls and Spout integration
class RivuletBrowserWindow {
public:
    struct Config {
        std::string startup_url = "https://www.google.com";
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
    int RunMessageLoop();
    void Shutdown();

    CefRefPtr<CefBrowser> GetBrowser() const { return browser_; }
    HWND GetWindowHandle() const { return hwnd_; }

private:
    // Window creation and management
    bool CreateMainWindow(const Config& config);
    void CreateControls();
    void CreateCefBrowser(); // Delayed CEF browser creation

    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
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
    
    // Input event handlers for off-screen rendering
    void OnMouseEvent(UINT message, WPARAM wParam, LPARAM lParam);
    void OnKeyEvent(UINT message, WPARAM wParam, LPARAM lParam);

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

        // CefRenderHandler methods (for off-screen rendering to Spout)
        void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        void OnPaint(CefRefPtr<CefBrowser> browser,
                    PaintElementType type,
                    const RectList& dirtyRects,
                    const void* buffer,
                    int width,
                    int height) override;

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
    WNDPROC edit_wndproc_old_;
    
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
    
    // Spout output dimensions
    int spout_width_;
    int spout_height_;
    
    // Off-screen rendering buffer for display
    std::vector<uint8_t> display_buffer_;
    bool has_new_frame_;

    // Control IDs
    static constexpr int ID_BACK = 1001;
    static constexpr int ID_FORWARD = 1002;
    static constexpr int ID_RELOAD = 1003;
    static constexpr int ID_STOP = 1004;
    static constexpr int ID_URL_EDIT = 1005;

    // Control dimensions
    static constexpr int BUTTON_WIDTH = 50;
    static constexpr int BUTTON_HEIGHT = 25;
    static constexpr int TOOLBAR_HEIGHT = 35;
    static constexpr int TOOLBAR_PADDING = 5;

    DISALLOW_COPY_AND_ASSIGN(RivuletBrowserWindow);
};

} // namespace Rivulet

#endif // RIVULET_BROWSER_WINDOW_H_