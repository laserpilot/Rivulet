// Simple test main - no CEF, just window + console
#include <windows.h>
#include <iostream>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // Console setup
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
    
    std::cout << "Simple Rivulet Test - Starting..." << std::endl;
    
    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SimpleRivuletTest";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    
    if (!RegisterClassW(&wc)) {
        std::cout << "Failed to register window class" << std::endl;
        return -1;
    }
    
    std::cout << "Window class registered" << std::endl;
    
    // Create window
    HWND hwnd = CreateWindowExW(
        0,
        L"SimpleRivuletTest",
        L"Simple Rivulet Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );
    
    if (!hwnd) {
        std::cout << "Failed to create window" << std::endl;
        return -1;
    }
    
    std::cout << "Window created successfully" << std::endl;
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    std::cout << "Window shown - entering message loop" << std::endl;
    
    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    std::cout << "Message loop ended" << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    FreeConsole();
    return 0;
}