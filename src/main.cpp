// Rivulet - Modern CEF-Spout Video Sharing Application
// main.cpp - Application entry point

#include "application.h"
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
    
    // Enable console for debugging in debug builds
#ifdef _DEBUG
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();
#endif

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

#ifdef _DEBUG
    // Keep console open in debug builds
    std::cout << "Press Enter to continue..." << std::endl;
    std::cin.get();
    FreeConsole();
#endif

    return 0;
}