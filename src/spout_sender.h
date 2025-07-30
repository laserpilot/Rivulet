// Rivulet - Modern CEF-Spout Video Sharing Application
// spout_sender.h - Spout2 sender integration

#pragma once

#include <Spout.h>
#include <d3d11.h>
#include <string>
#include <memory>

namespace Rivulet {

class D3D11Device;

class SpoutSender {
public:
    SpoutSender();
    ~SpoutSender();

    // Non-copyable
    SpoutSender(const SpoutSender&) = delete;
    SpoutSender& operator=(const SpoutSender&) = delete;

    bool Initialize(const std::string& name);
    void Shutdown();

    // Sending methods
    bool SendTexture(ID3D11Texture2D* texture, uint32_t width, uint32_t height);
    bool SendFrame(const void* pixels, uint32_t width, uint32_t height, GLenum format = GL_BGRA);

    // Status
    bool HasReceivers() const;
    std::string GetName() const { return sender_name_; }

    // Update sender properties
    bool SetName(const std::string& name);

private:
    spoutSender spout_;
    std::string sender_name_;
    bool initialized_;
    
    // Last frame info for change detection
    uint32_t last_width_;
    uint32_t last_height_;
};

} // namespace Rivulet