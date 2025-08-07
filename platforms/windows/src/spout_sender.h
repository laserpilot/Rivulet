// Rivulet - Modern CEF-Spout Video Sharing Application
// spout_sender.h - Spout2 sender integration

#pragma once

#include <SpoutDX.h>
#include <d3d11.h>
#include <string>
#include <memory>

namespace Rivulet {

class D3D11Device;

class RivuletSpoutSender {
public:
    RivuletSpoutSender();
    ~RivuletSpoutSender();

    // Non-copyable
    RivuletSpoutSender(const RivuletSpoutSender&) = delete;
    RivuletSpoutSender& operator=(const RivuletSpoutSender&) = delete;

    bool Initialize(const std::string& name, ID3D11Device* d3d_device = nullptr);
    void Shutdown();

    // Sending methods
    bool SendTexture(ID3D11Texture2D* texture, uint32_t width, uint32_t height);
    bool SendFrame(const void* pixels, uint32_t width, uint32_t height);

    // Status
    bool HasReceivers();
    std::string GetName() const { return sender_name_; }

    // Update sender properties
    bool SetName(const std::string& name);

private:
    spoutDX spout_;
    std::string sender_name_;
    bool initialized_;
    
    // Last frame info for change detection
    uint32_t last_width_;
    uint32_t last_height_;
};

} // namespace Rivulet