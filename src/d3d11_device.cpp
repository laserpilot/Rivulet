// Rivulet - Modern CEF-Spout Video Sharing Application
// d3d11_device.cpp - D3D11 device implementation

#include "d3d11_device.h"
#include <iostream>
#include <vector>

namespace Rivulet {

D3D11Device::D3D11Device() : initialized_(false) {
}

D3D11Device::~D3D11Device() {
    Shutdown();
}

bool D3D11Device::Initialize() {
    if (initialized_) return true;
    
    std::cout << "Creating D3D11 device..." << std::endl;
    
    // Device creation flags
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Feature levels to try
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    
    D3D_FEATURE_LEVEL featureLevel;
    
    // Try hardware first, then software
    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    
    HRESULT hr = E_FAIL;
    for (auto driverType : driverTypes) {
        hr = D3D11CreateDevice(
            nullptr,                    // adapter
            driverType,                 // driver type
            nullptr,                    // software
            createDeviceFlags,          // flags
            featureLevels,              // feature levels
            ARRAYSIZE(featureLevels),   // num feature levels
            D3D11_SDK_VERSION,          // SDK version
            &device_,                   // device
            &featureLevel,              // feature level
            &context_                   // context
        );
        
        if (SUCCEEDED(hr)) {
            std::cout << "✅ D3D11 device created with driver type: " << driverType << std::endl;
            break;
        }
    }
    
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create D3D11 device: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    // Get adapter information
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = device_.As(&dxgiDevice);
    if (SUCCEEDED(hr)) {
        hr = dxgiDevice->GetAdapter(&adapter_);
        if (SUCCEEDED(hr)) {
            DXGI_ADAPTER_DESC desc;
            adapter_->GetDesc(&desc);
            std::wcout << L"Using adapter: " << desc.Description << std::endl;
        }
    }
    
    initialized_ = true;
    std::cout << "✅ D3D11 device initialized successfully" << std::endl;
    return true;
}

void D3D11Device::Shutdown() {
    if (!initialized_) return;
    
    adapter_.Reset();
    context_.Reset();
    device_.Reset();
    
    initialized_ = false;
    std::cout << "D3D11 device shut down" << std::endl;
}

std::string D3D11Device::GetAdapterName() const {
    if (!adapter_) return "Unknown";
    
    DXGI_ADAPTER_DESC desc;
    if (SUCCEEDED(adapter_->GetDesc(&desc))) {
        // Convert wide string to string
        char buffer[256];
        wcstombs_s(nullptr, buffer, sizeof(buffer), desc.Description, _TRUNCATE);
        return std::string(buffer);
    }
    
    return "Unknown";
}

bool D3D11Device::CreateSharedTexture(uint32_t width, uint32_t height, DXGI_FORMAT format,
                                     ID3D11Texture2D** texture, HANDLE* shared_handle) {
    if (!device_ || !texture) return false;
    
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;  // Enable sharing
    
    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to create shared texture: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    // Get shared handle
    if (shared_handle) {
        ComPtr<IDXGIResource> dxgiResource;
        hr = tex.As(&dxgiResource);
        if (SUCCEEDED(hr)) {
            hr = dxgiResource->GetSharedHandle(shared_handle);
            if (FAILED(hr)) {
                std::cerr << "❌ Failed to get shared handle: 0x" << std::hex << hr << std::endl;
                return false;
            }
        }
    }
    
    *texture = tex.Detach();
    return true;
}

bool D3D11Device::OpenSharedTexture(HANDLE shared_handle, ID3D11Texture2D** texture) {
    if (!device_ || !shared_handle || !texture) return false;
    
    ComPtr<ID3D11Resource> resource;
    HRESULT hr = device_->OpenSharedResource(shared_handle, IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        std::cerr << "❌ Failed to open shared resource: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    ComPtr<ID3D11Texture2D> tex;
    hr = resource.As(&tex);
    if (FAILED(hr)) {
        std::cerr << "❌ Resource is not a texture2D: 0x" << std::hex << hr << std::endl;
        return false;
    }
    
    *texture = tex.Detach();
    return true;
}

} // namespace Rivulet