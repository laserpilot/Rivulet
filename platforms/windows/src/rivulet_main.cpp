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

// CEF application class with GPU adapter synchronization
class RivuletCefApp : public CefApp {
public:
    RivuletCefApp(const std::string& gpu_adapter_luid) : gpu_adapter_luid_(gpu_adapter_luid) {}

    // Add command-line arguments for GPU synchronization
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                     CefRefPtr<CefCommandLine> command_line) override {
        if (!gpu_adapter_luid_.empty()) {
            command_line->AppendSwitch("enable-gpu");
            command_line->AppendSwitch("enable-gpu-compositing");  
            command_line->AppendSwitch("enable-shared-texture");
            command_line->AppendSwitchWithValue("gpu-adapter-luid", gpu_adapter_luid_);
            
            std::cout << "🔧 Added CEF GPU arguments: --gpu-adapter-luid=" << gpu_adapter_luid_ << std::endl;
        }
    }

    IMPLEMENT_REFCOUNTING(RivuletCefApp);

private:
    std::string gpu_adapter_luid_;
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
    
    try {
        // STEP 1: Pre-initialize DirectX to get GPU adapter LUID for CEF synchronization
        std::cout << "🔍 Pre-initializing DirectX to determine GPU adapter for CEF..." << std::endl;
        auto browser_window = std::make_unique<Rivulet::RivuletBrowserWindow>(hInstance);
        
        Rivulet::RivuletBrowserWindow::Config config;
        config.startup_url = "https://www.google.com";
        config.window_height = 600;  // Desired window content height 
        // window_width will be calculated to match spout aspect ratio
        config.spout_width = 1920;   // High-resolution Spout output
        config.spout_height = 1080;  // Independent of window display size
        config.window_title = "Rivulet - Professional Browser";
        
        // Pre-initialize only the parts needed to get LUID
        if (!browser_window->PreInitializeForLuid()) {
            std::cerr << "❌ Failed to pre-initialize for LUID detection" << std::endl;
            return -1;
        }
        
        // Get the adapter LUID string for CEF
        std::string adapter_luid = browser_window->GetSelectedAdapterLuidString();
        std::cout << "🎯 GPU Adapter LUID for CEF: " << adapter_luid << std::endl;
        
        // STEP 2: Now initialize CEF with the adapter LUID  
        std::cout << "🚀 Initializing CEF with synchronized GPU adapter..." << std::endl;
        
        CefRefPtr<RivuletCefApp> app = new RivuletCefApp(adapter_luid);
        if (!CefInitialize(main_args, settings, app, nullptr)) {
            std::cerr << "❌ Failed to initialize CEF with GPU synchronization" << std::endl;
            return -1;
        }
        
        std::cout << "✅ CEF initialized with GPU adapter synchronization" << std::endl;
        
        // STEP 3: Complete the browser window initialization
        std::cout << "🎯 Configuration:" << std::endl;
        std::cout << "   Spout Output: " << config.spout_width << "x" << config.spout_height << std::endl;
        std::cout << "   Window will be sized to match aspect ratio" << std::endl;
        std::cout << "   GPU Adapter: synchronized between CEF and DirectX" << std::endl;
        
        if (!browser_window->CompleteInitialization(config)) {
            std::cerr << "❌ Failed to complete browser window initialization" << std::endl;
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