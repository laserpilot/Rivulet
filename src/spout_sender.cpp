// Rivulet - Modern CEF-Spout Video Sharing Application
// spout_sender.cpp - Spout2 sender implementation

#include "spout_sender.h"
#include <iostream>

namespace Rivulet {

RivuletSpoutSender::RivuletSpoutSender() 
    : initialized_(false)
    , last_width_(0)
    , last_height_(0) {
}

RivuletSpoutSender::~RivuletSpoutSender() {
    Shutdown();
}

bool RivuletSpoutSender::Initialize(const std::string& name) {
    if (initialized_) return true;
    
    std::cout << "Initializing Spout sender: " << name << std::endl;
    
    sender_name_ = name;
    
    // Initialize Spout sender
    if (!spout_.SetSenderName(sender_name_.c_str())) {
        std::cerr << "❌ Failed to create Spout sender" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "✅ Spout sender initialized: " << name << std::endl;
    return true;
}

void RivuletSpoutSender::Shutdown() {
    if (!initialized_) return;
    
    spout_.ReleaseSender();
    initialized_ = false;
    
    std::cout << "Spout sender shut down: " << sender_name_ << std::endl;
}

bool RivuletSpoutSender::SendTexture(ID3D11Texture2D* texture, uint32_t width, uint32_t height) {
    if (!initialized_ || !texture) return false;
    
    // Check if we need to update sender dimensions
    if (width != last_width_ || height != last_height_) {
        std::cout << "Updating Spout sender dimensions: " << width << "x" << height << std::endl;
        // SpoutDX automatically handles dimension updates
        last_width_ = width;
        last_height_ = height;
    }
    
    // Send the D3D11 texture directly
    // This is the zero-copy path!
    bool success = spout_.SendTexture(texture);
    
    if (!success) {
        std::cerr << "❌ Failed to send texture via Spout" << std::endl;
        return false;
    }
    
    // Log periodically (every 60 frames)
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 60 == 0) {
        int receiver_count = spout_.GetSenderCount();
        std::cout << "📺 Spout frame sent: " << width << "x" << height 
                  << " Frame #" << frame_count 
                  << " Receivers: " << receiver_count << std::endl;
    }
    
    return true;
}

bool RivuletSpoutSender::SendFrame(const void* pixels, uint32_t width, uint32_t height) {
    if (!initialized_ || !pixels) return false;
    
    // Check if we need to update sender dimensions
    if (width != last_width_ || height != last_height_) {
        std::cout << "Updating Spout sender dimensions: " << width << "x" << height << std::endl;
        // SpoutDX automatically handles dimension updates
        last_width_ = width;
        last_height_ = height;
    }
    
    // Send pixel data (fallback method)
    bool success = spout_.SendImage((const unsigned char*)pixels, width, height);
    
    if (!success) {
        std::cerr << "❌ Failed to send image via Spout" << std::endl;
        return false;
    }
    
    // Log periodically
    static int frame_count = 0;
    frame_count++;
    if (frame_count % 60 == 0) {
        int receiver_count = spout_.GetSenderCount();
        std::cout << "📺 Spout image sent: " << width << "x" << height 
                  << " Frame #" << frame_count 
                  << " Receivers: " << receiver_count << std::endl;
    }
    
    return true;
}

bool RivuletSpoutSender::HasReceivers() {
    if (!initialized_) return false;
    return spout_.GetSenderCount() > 0;
}

bool RivuletSpoutSender::SetName(const std::string& name) {
    if (name == sender_name_) return true;
    
    // Release current sender
    if (initialized_) {
        spout_.ReleaseSender();
    }
    
    // Create new sender with new name
    sender_name_ = name;
    bool success = spout_.SetSenderName(sender_name_.c_str());
    
    if (success) {
        std::cout << "✅ Spout sender renamed to: " << name << std::endl;
    } else {
        std::cerr << "❌ Failed to rename Spout sender to: " << name << std::endl;
    }
    
    return success;
}

} // namespace Rivulet