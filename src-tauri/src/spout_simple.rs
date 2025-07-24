// Windows Spout implementation for Tauri using C++ bridge
// Mirrors syphon_simple.rs functionality for cross-platform compatibility

use std::ffi::{CString, c_char, c_void};
use std::sync::Mutex;

#[cfg(target_os = "windows")]
use winapi::um::d3d11::{ID3D11Device, ID3D11DeviceContext};
#[cfg(target_os = "windows")]
use winapi::shared::winerror::{SUCCEEDED, FAILED};

// C interface to C++ Spout bridge
#[repr(C)]
pub struct SpoutServerState {
    _private: [u8; 0],
}

extern "C" {
    // Core Spout server functions - mirror syphon_bridge.m
    fn spout_server_create(
        name: *const std::os::raw::c_char,
        d3d_device: *const std::ffi::c_void,
    ) -> *mut SpoutServerState;
    
    fn spout_server_publish_frame(
        state: *mut SpoutServerState,
        data: *const u8,
        width: u32,
        height: u32,
    ) -> bool;
    
    fn spout_server_publish_texture(
        state: *mut SpoutServerState,
        texture_handle: *const std::ffi::c_void,
    ) -> bool;
    
    fn spout_server_has_clients(state: *mut SpoutServerState) -> bool;
    fn spout_server_stop(state: *mut SpoutServerState);
    
    // Test texture creation - mirrors create_test_iosurface()
    fn create_test_d3d11_texture(width: u32, height: u32) -> *const std::ffi::c_void;
    
    // Screen capture functions (implemented in screencapture_d3d11.cpp)
    fn spout_server_start_screen_capture() -> bool;
    fn spout_server_start_window_capture(window_id: u32) -> bool;
    fn spout_server_start_application_capture() -> bool;
    fn spout_server_start_application_window_capture(window_id: u32) -> bool;
    fn spout_server_start_content_only_window_capture(window_id: u32) -> bool;
    fn spout_server_has_screen_frame() -> bool;
    fn spout_server_publish_screen_capture(state: *mut SpoutServerState) -> bool;
    fn spout_server_stop_screen_capture();
    
    // Window detection functions (implemented in screencapture_d3d11.cpp)
    fn get_frontmost_window_id() -> u32;
    fn get_our_window_info() -> u32;
    fn get_window_by_title_prefix(prefix: *const c_char) -> u32;
}

#[cfg(target_os = "windows")]
pub struct SpoutOutput {
    server_state: *mut SpoutServerState,
    name: String,
    frame_count: u64,
    d3d_device: Option<*mut ID3D11Device>,
    is_screen_capture_active: bool,
}

#[cfg(target_os = "windows")]
unsafe impl Send for SpoutOutput {}
#[cfg(target_os = "windows")]
unsafe impl Sync for SpoutOutput {}

#[cfg(target_os = "windows")]
impl SpoutOutput {
    /// Create new Spout output server
    /// Mirrors SyphonOutput::new() from syphon_simple.rs
    pub fn new(name: String) -> Self {
        log::info!("🎬 Creating Spout output: {}", name);
        
        let c_name = CString::new(name.clone()).unwrap_or_else(|_| CString::new("SpoutOutput").unwrap());
        
        unsafe {
            // Create Spout server with no D3D device (bridge will create its own)
            let server_state = spout_server_create(c_name.as_ptr(), std::ptr::null());
            
            if server_state.is_null() {
                log::error!("❌ Failed to create Spout server: {}", name);
                // Return a "failed" instance that operations will safely fail on
                return SpoutOutput {
                    server_state: std::ptr::null_mut(),
                    name,
                    frame_count: 0,
                    d3d_device: None,
                    is_screen_capture_active: false,
                };
            }
            
            log::info!("✅ Spout server created successfully: {}", name);
            
            SpoutOutput {
                server_state,
                name,
                frame_count: 0,
                d3d_device: None,
                is_screen_capture_active: false,
            }
        }
    }
    
    /// Update frame with RGBA bitmap data
    /// Mirrors SyphonOutput::update_frame() from syphon_simple.rs
    pub fn update_frame(&self, data: &[u8], width: u32, height: u32) -> bool {
        if self.server_state.is_null() {
            log::warn!("⚠️ Spout server not initialized, cannot update frame");
            return false;
        }
        
        if data.is_empty() || width == 0 || height == 0 {
            log::warn!("⚠️ Invalid frame data: {} bytes, {}x{}", data.len(), width, height);
            return false;
        }
        
        let expected_size = (width * height * 4) as usize; // 4 bytes per RGBA pixel
        if data.len() != expected_size {
            log::warn!("⚠️ Frame data size mismatch: expected {} bytes, got {}", expected_size, data.len());
            return false;
        }
        
        unsafe {
            let success = spout_server_publish_frame(
                self.server_state,
                data.as_ptr(),
                width,
                height
            );
            
            if success {
                // Note: We don't increment frame_count here as self is immutable
                // This matches the pattern from syphon_simple.rs
                log::debug!("📋 Spout frame published: {}x{} ({} bytes)", width, height, data.len());
            } else {
                log::error!("❌ Failed to publish Spout frame: {}x{}", width, height);
            }
            
            success
        }
    }
    
    /// Check if clients are connected
    /// Mirrors SyphonOutput::has_clients() from syphon_simple.rs
    pub fn has_clients(&self) -> bool {
        if self.server_state.is_null() {
            return false;
        }
        
        unsafe {
            spout_server_has_clients(self.server_state)
        }
    }
    
    /// Publish test D3D11 texture for zero-copy demonstration  
    /// Mirrors SyphonOutput::publish_test_iosurface() from syphon_simple.rs
    pub fn publish_test_d3d11_texture(&self, width: u32, height: u32) -> bool {
        if self.server_state.is_null() {
            log::warn!("⚠️ Spout server not initialized, cannot publish test texture");
            return false;
        }
        
        log::info!("🎯 Publishing test D3D11 texture: {}x{}", width, height);
        
        unsafe {
            // Create test D3D11 texture
            let texture_handle = create_test_d3d11_texture(width, height);
            if texture_handle.is_null() {
                log::error!("❌ Failed to create test D3D11 texture");
                return false;
            }
            
            // Publish the texture
            let success = spout_server_publish_texture(self.server_state, texture_handle);
            if success {
                log::info!("✅ Test D3D11 texture published successfully: {}x{}", width, height);
            } else {
                log::error!("❌ Failed to publish test D3D11 texture");
            }
            
            success
        }
    }
    
    /// Stop the Spout server and cleanup
    /// Mirrors SyphonOutput::stop() from syphon_simple.rs
    pub fn stop(&self) {
        if self.server_state.is_null() {
            log::info!("🛑 Spout server already stopped or never initialized");
            return;
        }
        
        log::info!("🛑 Stopping Spout server: {}", self.name);
        
        unsafe {
            spout_server_stop(self.server_state);
        }
        
        log::info!("✅ Spout server stopped: {}", self.name);
    }
    
    // Screen capture methods (implemented with DXGI Desktop Duplication)
    // These mirror the screen capture functionality from syphon_simple.rs
    
    /// Start desktop screen capture using DXGI Desktop Duplication
    pub fn start_screen_capture(&self) -> bool {
        log::info!("🎬 Starting Windows desktop screen capture (DXGI Desktop Duplication)");
        
        unsafe {
            let success = spout_server_start_screen_capture();
            if success {
                // Note: We can't modify self.is_screen_capture_active here as self is immutable
                // The state is managed globally in the C++ layer
                log::info!("✅ Spout screen capture started successfully");
            } else {
                log::error!("❌ Failed to start Spout screen capture");
            }
            success
        }
    }
    
    /// Start window-specific capture using Win32 API
    pub fn start_window_capture(&self, window_id: u32) -> bool {
        log::info!("🎯 Starting Windows window-specific capture for HWND: {}", window_id);
        
        unsafe {
            let success = spout_server_start_window_capture(window_id);
            if success {
                log::info!("✅ Spout window capture started for HWND: {}", window_id);
            } else {
                log::error!("❌ Failed to start Spout window capture for HWND: {}", window_id);
            }
            success
        }
    }
    
    /// Start application-based capture
    pub fn start_application_capture(&self) -> bool {
        log::info!("🎯 Starting Windows application-based capture");
        
        unsafe {
            let success = spout_server_start_application_capture();
            if success {
                log::info!("✅ Spout application capture started");
            } else {
                log::error!("❌ Failed to start Spout application capture");
            }
            success
        }
    }
    
    /// Start application window capture (Phase E.2 equivalent)
    pub fn start_application_window_capture(&self, window_id: u32) -> bool {
        log::info!("🎯 Starting Windows application window capture for HWND: {}", window_id);
        
        unsafe {
            let success = spout_server_start_application_window_capture(window_id);
            if success {
                log::info!("✅ Spout application window capture started for HWND: {}", window_id);
            } else {
                log::error!("❌ Failed to start Spout application window capture for HWND: {}", window_id);
            }
            success
        }
    }
    
    /// Start content-only window capture (Phase E.3 equivalent)
    pub fn start_content_only_window_capture(&self, window_id: u32) -> bool {
        log::info!("🎯 Starting Windows content-only window capture for HWND: {}", window_id);
        
        unsafe {
            let success = spout_server_start_content_only_window_capture(window_id);
            if success {
                log::info!("✅ Spout content-only window capture started for HWND: {}", window_id);
            } else {
                log::error!("❌ Failed to start Spout content-only window capture for HWND: {}", window_id);
            }
            success
        }
    }
    
    /// Check if screen frames are available
    pub fn has_screen_frames(&self) -> bool {
        unsafe {
            spout_server_has_screen_frame()
        }
    }
    
    /// Publish captured screen frame via Spout
    pub fn publish_screen_capture(&self) -> bool {
        if self.server_state.is_null() {
            return false;
        }
        
        unsafe {
            let success = spout_server_publish_screen_capture(self.server_state as *mut c_void);
            if success {
                log::debug!("📋 Spout screen frame published successfully");
            }
            success
        }
    }
    
    /// Stop screen capture and cleanup
    pub fn stop_screen_capture(&self) {
        log::info!("🛑 Stopping Windows screen capture");
        
        unsafe {
            spout_server_stop_screen_capture();
        }
        
        log::info!("✅ Windows screen capture stopped");
    }
    
    // Window detection methods (implemented with Win32 API)
    // These mirror the window detection functionality from syphon_simple.rs
    
    /// Get frontmost window ID using Win32 API
    pub fn get_frontmost_window_id(&self) -> u32 {
        log::info!("🔍 Getting frontmost window ID (Win32 implementation)");
        
        unsafe {
            let window_id = get_frontmost_window_id();
            if window_id != 0 {
                log::info!("✅ Found frontmost window: HWND={}", window_id);
            } else {
                log::warn!("⚠️ No frontmost window found");
            }
            window_id
        }
    }
    
    /// Get our application window info using Win32 API
    pub fn get_our_window_info(&self) -> u32 {
        log::info!("🎯 Getting our application window info (Win32 implementation)");
        
        unsafe {
            let window_id = get_our_window_info();
            if window_id != 0 {
                log::info!("✅ Found our application window: HWND={}", window_id);
            } else {
                log::warn!("⚠️ Could not find our application window");
            }
            window_id
        }
    }
    
    /// Get window by title prefix using Win32 API
    pub fn get_window_by_title_prefix(&self, prefix: &str) -> u32 {
        log::info!("🔍 Searching for window with title prefix: \"{}\"", prefix);
        
        let c_prefix = CString::new(prefix).unwrap_or_else(|_| CString::new("").unwrap());
        
        unsafe {
            let window_id = get_window_by_title_prefix(c_prefix.as_ptr());
            if window_id != 0 {
                log::info!("✅ Found window with prefix \"{}\": HWND={}", prefix, window_id);
            } else {
                log::warn!("⚠️ No window found with title prefix: \"{}\"", prefix);
            }
            window_id
        }
    }
}

#[cfg(target_os = "windows")]
impl Drop for SpoutOutput {
    fn drop(&mut self) {
        // Cleanup is handled by the stop() method
        // The C++ bridge will handle memory cleanup when spout_server_stop() is called
        if !self.server_state.is_null() {
            self.stop();
        }
    }
}

// Unit tests for Windows Spout functionality
#[cfg(all(test, target_os = "windows"))]
mod tests {
    use super::*;
    
    #[test]
    fn test_spout_output_creation() {
        let output = SpoutOutput::new("Test Spout Server".to_string());
        assert!(!output.server_state.is_null(), "Spout server should be created");
        assert_eq!(output.name, "Test Spout Server");
        assert_eq!(output.frame_count, 0);
    }
    
    #[test]
    fn test_spout_frame_publishing() {
        let output = SpoutOutput::new("Test Frame Publisher".to_string());
        
        // Create test RGBA frame (100x100 red pixels)
        let width = 100u32;
        let height = 100u32;
        let mut frame_data = Vec::with_capacity((width * height * 4) as usize);
        
        for _ in 0..(width * height) {
            frame_data.extend_from_slice(&[255, 0, 0, 255]); // Red RGBA
        }
        
        let success = output.update_frame(&frame_data, width, height);
        assert!(success, "Frame publishing should succeed");
    }
    
    #[test]
    fn test_spout_client_detection() {
        let output = SpoutOutput::new("Test Client Detection".to_string());
        
        // Client detection should work even with no clients
        let _has_clients = output.has_clients(); // May be true or false
        // We don't assert a specific value since it depends on external receivers
    }
    
    #[test]
    fn test_spout_test_texture() {
        let output = SpoutOutput::new("Test D3D11 Texture".to_string());
        
        // Note: This may fail if D3D11 is not available on the test system
        // In a real test environment, we'd check for D3D11 availability first
        let _success = output.publish_test_d3d11_texture(1920, 1080);
        // We don't assert success since it depends on D3D11 availability
    }
}