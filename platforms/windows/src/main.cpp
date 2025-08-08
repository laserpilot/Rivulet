// Rivulet - Modern CEF-Spout Video Sharing Application
// main.cpp - Application entry point

#include "application.h"
#include "include/cef_app.h"
#include "rivulet_browser_window.h"
#include <windows.h>
#include <iostream>
#include <io.h>
#include <fcntl.h>

// Console control functions
void ShowConsoleWindow() {
    HWND console_hwnd = GetConsoleWindow();
    if (console_hwnd) {
        ShowWindow(console_hwnd, SW_SHOW);
    }
}

void HideConsoleWindow() {
    HWND console_hwnd = GetConsoleWindow();
    if (console_hwnd) {
        ShowWindow(console_hwnd, SW_HIDE);
    }
}

void CloseConsoleWindow() {
    FreeConsole();
}

bool IsConsoleVisible() {
    HWND console_hwnd = GetConsoleWindow();
    if (console_hwnd) {
        return IsWindowVisible(console_hwnd);
    }
    return false;
}

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
    
    // Parse command line arguments
    std::wstring cmdLine(lpCmdLine ? lpCmdLine : L"");
    bool showConsole = cmdLine.find(L"--console") != std::wstring::npos;
    bool verboseLogging = cmdLine.find(L"--verbose") != std::wstring::npos;
    bool hideConsoleOnStart = cmdLine.find(L"--hide-console") != std::wstring::npos;
    
    // Handle CEF subprocesses - this prevents infinite console creation!
    CefMainArgs main_args(hInstance);
    int exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (exit_code >= 0) {
        // This was a subprocess, exit immediately
        return exit_code;
    }
    
    // Enable console for debugging - but allow hiding
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
    
    // Set verbose logging based on command line
    Rivulet::RivuletBrowserWindow::SetVerboseLogging(verboseLogging);
    
    if (verboseLogging) {
        std::cout << "📝 Verbose logging enabled" << std::endl;
    }
    
    // Hide console if requested (after showing startup messages)
    if (hideConsoleOnStart) {
        std::cout << "🔇 Hiding console window (use F12 to toggle)" << std::endl;
        Sleep(2000); // Give user time to see the message
        HideConsoleWindow();
    }

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
        if (!hideConsoleOnStart && IsConsoleVisible()) {
            std::cout << "Press Enter to continue..." << std::endl;
            std::cin.get();
        }
        return -1;
    }

    // Don't automatically prompt to keep console open - let user control with F12
    return 0;
}