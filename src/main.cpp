// Rivulet - Modern CEF-Spout Video Sharing Application
// main.cpp - Application entry point

#include "application.h"
#include "include/cef_app.h"
#include <windows.h>
#include <iostream>

// Force high-performance GPU on hybrid systems
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow) {
    
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    
    // Handle CEF subprocesses - this prevents infinite console creation!
    CefMainArgs main_args(hInstance);
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        // This was a subprocess, exit immediately
        return exit_code;
    }
    
    // Enable console for debugging - simplified approach
    AllocConsole();
    
    // Set console to handle UTF-8 properly
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // Redirect stdout, stderr to console
    FILE* pCout;
    FILE* pCerr;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);

    std::cout << "🚀 Rivulet - Modern CEF-Spout Video Sharing" << std::endl;
    std::cout << "Starting application..." << std::endl;

    try {
        // Create and run application
        auto app = std::make_unique<Rivulet::Application>(hInstance);
        
        if (!app->Initialize()) {
            std::cerr << "❌ Failed to initialize application" << std::endl;
            return -1;
        }

        std::cout << "✅ Application initialized successfully" << std::endl;
        
        // Run message loop
        int result = app->Run();
        
        std::cout << "Application exiting with code: " << result << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Application error: " << e.what() << std::endl;
        return -1;
    }

    // Keep console open
    std::cout << "Press Enter to continue..." << std::endl;
    std::cin.get();
    FreeConsole();

    return 0;
}