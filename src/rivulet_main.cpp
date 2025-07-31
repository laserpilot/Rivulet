// Rivulet - Modern CEF-Spout Video Sharing Application
// rivulet_main.cpp - Main application entry point using professional CEF browser

#include "rivulet_browser_window.h"

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

#include <windows.h>
#include <iostream>

// Force high-performance GPU on hybrid systems
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Simple CEF application class
class RivuletCefApp : public CefApp {
public:
    RivuletCefApp() {}

    IMPLEMENT_REFCOUNTING(RivuletCefApp);
};

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    // Handle CEF subprocesses first - exit immediately for subprocesses
    CefMainArgs main_args(hInstance);
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        return exit_code; // This was a subprocess, exit silently without console
    }
    
    // Only create console for main process
    AllocConsole();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    FILE* pCout;
    FILE* pCerr;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);

    std::cout << "🚀 Rivulet - Professional CEF-Spout Video Sharing" << std::endl;
    std::cout << "Starting main application process..." << std::endl;
    
    // Initialize CEF
    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    
    // Set paths
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string exe_dir = exe_path;
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));
    
    CefString(&settings.cache_path).FromASCII((exe_dir + "\\cache").c_str());
    CefString(&settings.log_file).FromASCII((exe_dir + "\\cef.log").c_str());
    settings.log_severity = LOGSEVERITY_INFO;
    
    CefRefPtr<RivuletCefApp> app = new RivuletCefApp();
    if (!CefInitialize(main_args, settings, app, nullptr)) {
        std::cerr << "❌ Failed to initialize CEF" << std::endl;
        return -1;
    }
    
    std::cout << "✅ CEF initialized successfully" << std::endl;
    
    try {
        // Create browser window
        auto browser_window = std::make_unique<Rivulet::RivuletBrowserWindow>(hInstance);
        
        Rivulet::RivuletBrowserWindow::Config config;
        config.startup_url = "https://www.google.com";
        config.window_height = 600;  // Desired window content height 
        // window_width will be calculated to match spout aspect ratio
        config.spout_width = 1920;   // High-resolution Spout output
        config.spout_height = 1080;  // Independent of window display size
        config.window_title = "Rivulet - Professional Browser";
        
        std::cout << "🎯 Configuration:" << std::endl;
        std::cout << "   Spout Output: " << config.spout_width << "x" << config.spout_height << std::endl;
        std::cout << "   Window will be sized to match aspect ratio" << std::endl;
        
        if (!browser_window->Initialize(config)) {
            std::cerr << "❌ Failed to initialize browser window" << std::endl;
            CefShutdown();
            return -1;
        }
        
        std::cout << "✅ Browser window initialized" << std::endl;
        std::cout << "🌐 Application ready - professional browser with Spout integration!" << std::endl;
        
        // Run message loop
        int result = browser_window->RunMessageLoop();
        
        // Cleanup
        browser_window->Shutdown();
        browser_window.reset();
        
        // Shutdown CEF
        CefShutdown();
        
        std::cout << "Application exiting with code: " << result << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Application error: " << e.what() << std::endl;
        CefShutdown();
        return -1;
    }
    
    // Keep console open for debugging
    std::cout << "Press Enter to continue..." << std::endl;
    std::cin.get();
    FreeConsole();
    
    return 0;
}