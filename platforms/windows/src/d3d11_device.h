// Rivulet - Modern CEF-Spout Video Sharing Application
// d3d11_device.h - D3D11 device management

#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <memory>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Rivulet {

class D3D11Device {
public:
    D3D11Device();
    ~D3D11Device();

    // Non-copyable
    D3D11Device(const D3D11Device&) = delete;
    D3D11Device& operator=(const D3D11Device&) = delete;

    bool Initialize();
    void Shutdown();

    // Accessors
    ID3D11Device* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext* GetContext() const { return context_.Get(); }
    
    // Device information
    std::string GetAdapterName() const;
    
    // Texture management
    bool CreateSharedTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, 
                           ID3D11Texture2D** texture, HANDLE* shared_handle);
    bool OpenSharedTexture(HANDLE shared_handle, ID3D11Texture2D** texture);

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIAdapter> adapter_;
    
    bool initialized_;
};

} // namespace Rivulet