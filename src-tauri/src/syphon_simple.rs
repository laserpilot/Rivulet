// Real Syphon implementation for Tauri using C bridge
use std::ffi::{CString, c_char};
use std::sync::Mutex;

// MockIOSurface is no longer needed since we use real screen capture
// #[cfg(target_os = "macos")]
// use crate::screen_capture_simple::MockIOSurface;

#[cfg(target_os = "macos")]
use cocoa::base::{id, nil};
#[cfg(target_os = "macos")]
use objc::{msg_send, sel, sel_impl};

// C interface to Objective-C Syphon bridge
#[repr(C)]
pub struct SyphonServerState {
    _private: [u8; 0],
}

extern "C" {
    // IMPORTANT: The C bridge function must be updated to accept the context handle.
    fn syphon_server_create(
        name: *const std::os::raw::c_char,
        ctx: *const std::ffi::c_void,
    ) -> *mut SyphonServerState;
    
    fn syphon_server_publish_frame(
        state: *mut SyphonServerState,
        data: *const u8,
        width: u32,
        height: u32,
    ) -> bool;
    fn syphon_server_publish_iosurface(
        state: *mut SyphonServerState,
        iosurface_ref: *const std::ffi::c_void,
    ) -> bool;
    fn syphon_server_has_clients(state: *mut SyphonServerState) -> bool;
    fn syphon_server_stop(state: *mut SyphonServerState);
    // ZERO-COPY: Create test IOSurface for demonstration
    fn create_test_iosurface(width: u32, height: u32) -> *const std::ffi::c_void;
    
    // REAL SCREEN CAPTURE FUNCTIONS (from screencapture_iosurface.m)
    fn syphon_server_start_screen_capture() -> bool;
    fn syphon_server_start_window_capture(window_id: u32) -> bool;       // DEPRECATED
    fn syphon_server_start_application_capture() -> bool;                // Application-based capture
    fn syphon_server_start_application_window_capture(window_id: u32) -> bool;  // PHASE E.2: Window-specific capture
    fn syphon_server_start_content_only_window_capture(window_id: u32) -> bool;  // PHASE E.3: Content-only capture
    fn syphon_server_publish_screen_capture(state: *mut SyphonServerState) -> bool;
    fn syphon_server_has_screen_frame() -> bool;
    fn syphon_server_stop_screen_capture();
    
    // PHASE D: Simple data bridge function
    fn get_frontmost_window_id() -> u32;
    
    // PHASE E: Get our own application's window information
    fn get_our_window_info() -> u32;
    
    // PHASE G: Get window by title prefix (for content window targeting)
    fn get_window_by_title_prefix(title_prefix: *const c_char) -> u32;
}

pub struct SyphonOutput {
    name: String,
    #[cfg(target_os = "macos")]
    server_state: Mutex<Option<*mut SyphonServerState>>,
    #[cfg(target_os = "macos")]
    gl_context: Option<id>, // Manages the NSOpenGLContext
    frame_count: std::sync::atomic::AtomicU64,
}

// Safe wrapper for SyphonServerState pointer
// SAFETY: SyphonServerState is managed through the Objective-C bridge which handles thread safety
#[cfg(target_os = "macos")]
unsafe impl Send for SyphonOutput {}
#[cfg(target_os = "macos")]
unsafe impl Sync for SyphonOutput {}

// SAFETY: The raw pointer is opaque and managed by Objective-C code
#[cfg(target_os = "macos")]
unsafe impl Send for SyphonServerState {}
#[cfg(target_os = "macos")]
unsafe impl Sync for SyphonServerState {}

impl SyphonOutput {
    pub fn new(name: String) -> Self {
        log::info!("Creating real SyphonOutput with name: {}", name);

        #[cfg(target_os = "macos")]
        {
            let mut gl_context: Option<id> = None;
            let server_state = unsafe {
                // 1. Create a dedicated headless OpenGL context for Syphon
                log::info!("🔧 Creating dedicated NSOpenGLContext for Syphon server...");
                let attrs = [
                    cocoa::appkit::NSOpenGLPFAAccelerated as u32,
                    cocoa::appkit::NSOpenGLPFANoRecovery as u32,
                    cocoa::appkit::NSOpenGLPFAColorSize as u32, 24,
                    cocoa::appkit::NSOpenGLPFAAlphaSize as u32, 8,
                    cocoa::appkit::NSOpenGLPFADepthSize as u32, 16,
                    0,
                ];

                let pixel_format: id = msg_send![objc::class!(NSOpenGLPixelFormat), alloc];
                let pixel_format: id = msg_send![pixel_format, initWithAttributes: attrs.as_ptr()];

                if pixel_format == nil {
                    log::error!("❌ Failed to create NSOpenGLPixelFormat");
                    return Self::new_empty(name);
                }

                let context: id = msg_send![objc::class!(NSOpenGLContext), alloc];
                let context: id = msg_send![context, initWithFormat: pixel_format shareContext: nil];
                let _: () = msg_send![pixel_format, release]; // Context retains it

                if context == nil {
                    log::error!("❌ Failed to create NSOpenGLContext");
                    return Self::new_empty(name);
                }
                gl_context = Some(context);

                // 2. Make the context current on this thread (CRITICAL for Rust/Tauri)
                let _: () = msg_send![context, makeCurrentContext];
                log::info!("✅ Dedicated NSOpenGLContext created and made current for Syphon");

                // 3. Get the CGLContextObj to pass to Syphon
                let cgl_context: *const std::ffi::c_void = msg_send![context, CGLContextObj];

                // 4. Create the Syphon server with the valid dedicated context
                let c_name = CString::new(name.clone()).unwrap();
                syphon_server_create(c_name.as_ptr(), cgl_context)
            };

            if server_state.is_null() {
                log::error!("❌ Failed to create Syphon server state - OpenGL context issue");
                // Clean up context if server creation failed
                if let Some(context) = gl_context {
                    unsafe {
                        let _: () = msg_send![context, release];
                    }
                }
                Self {
                    name,
                    server_state: Mutex::new(None),
                    gl_context: None,
                    frame_count: std::sync::atomic::AtomicU64::new(0),
                }
            } else {
                log::info!("✅ Syphon server state created successfully with dedicated context");
                Self {
                    name,
                    server_state: Mutex::new(Some(server_state)),
                    gl_context,
                    frame_count: std::sync::atomic::AtomicU64::new(0),
                }
            }
        }

        #[cfg(not(target_os = "macos"))]
        {
            Self {
                name,
                frame_count: std::sync::atomic::AtomicU64::new(0),
            }
        }
    }

    #[cfg(target_os = "macos")]
    fn new_empty(name: String) -> Self {
        Self {
            name,
            server_state: Mutex::new(None),
            gl_context: None,
            frame_count: std::sync::atomic::AtomicU64::new(0),
        }
    }

    /// High-performance zero-copy IOSurface publishing (macOS only) - Now handled by screen capture
    /// This method is kept for compatibility but screen capture is the primary zero-copy path
    #[cfg(target_os = "macos")]
    pub fn publish_iosurface_raw(&self, iosurface_ref: *const std::ffi::c_void) -> bool {
        if let Ok(server_guard) = self.server_state.lock() {
            if let Some(server_state) = *server_guard {
                let success = unsafe {
                    syphon_server_publish_iosurface(server_state, iosurface_ref)
                };

                if success {
                    let count = self.frame_count.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                    if count % 60 == 0 { // Log every 60 frames
                        log::info!("🚀 Published zero-copy Syphon IOSurface frame {}", count);
                    }
                } else {
                    log::warn!("❌ Failed to publish zero-copy IOSurface frame");
                }

                success
            } else {
                log::error!("❌ Syphon server state not initialized for IOSurface publishing");
                false
            }
        } else {
            log::error!("❌ Failed to lock Syphon server state for IOSurface publishing");
            false
        }
    }

    /// TRUE ZERO-COPY: Create and publish test IOSurface (BREAKTHROUGH METHOD)
    #[cfg(target_os = "macos")]
    pub fn publish_test_iosurface(&self, width: u32, height: u32) -> bool {
        if let Ok(server_guard) = self.server_state.lock() {
            if let Some(server_state) = *server_guard {
                // Create a real IOSurface for testing
                let iosurface_ref = unsafe { create_test_iosurface(width, height) };

                if iosurface_ref.is_null() {
                    log::error!("❌ Failed to create test IOSurface");
                    return false;
                }

                // Publish using true zero-copy path
                let success = unsafe {
                    syphon_server_publish_iosurface(server_state, iosurface_ref)
                };

                if success {
                    let count = self.frame_count.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                    if count % 30 == 0 { // Log every 30 frames
                        log::info!("🎯 TRUE ZERO-COPY: Published IOSurface frame {}: {}x{}",
                                   count, width, height);
                    }
                } else {
                    log::warn!("❌ Failed to publish true zero-copy IOSurface frame: {}x{}",
                               width, height);
                }

                // IOSurface will be released by the system when no longer referenced
                success
            } else {
                log::error!("❌ Syphon server state not initialized for true zero-copy");
                false
            }
        } else {
            log::error!("❌ Failed to lock Syphon server state for true zero-copy");
            false
        }
    }

    /// REAL SCREEN CAPTURE: Initialize AVFoundation screen capture system
    #[cfg(target_os = "macos")]
    pub fn start_screen_capture(&self) -> bool {
        log::info!("🎬 Starting real AVFoundation screen capture");
        let success = unsafe { syphon_server_start_screen_capture() };
        if success {
            log::info!("✅ Screen capture initialized successfully");
        } else {
            log::error!("❌ Failed to initialize screen capture");
        }
        success
    }

    /// WINDOW CAPTURE: Initialize AVFoundation window-specific capture system
    #[cfg(target_os = "macos")]
    pub fn start_window_capture(&self, window_id: u32) -> bool {
        log::warn!("⚠️ start_window_capture is DEPRECATED - use start_application_capture instead");
        log::info!("🎯 Starting AVFoundation window capture for CGWindowID: {}", window_id);
        let success = unsafe { syphon_server_start_window_capture(window_id) };
        if success {
            log::info!("✅ Window capture initialized successfully for window {}", window_id);
        } else {
            log::error!("❌ Failed to initialize window capture for window {}", window_id);
        }
        success
    }

    /// NEW: Start application-based capture using ScreenCaptureKit (PREFERRED METHOD)
    #[cfg(target_os = "macos")]
    pub fn start_application_capture(&self) -> bool {
        log::info!("🎯 Starting ScreenCaptureKit application-based capture");
        let success = unsafe { syphon_server_start_application_capture() };
        if success {
            log::info!("✅ Application capture initialized successfully");
        } else {
            log::error!("❌ Failed to initialize application capture");
        }
        success
    }

    /// PHASE E.2: Start window-specific capture using ScreenCaptureKit  
    #[cfg(target_os = "macos")]
    pub fn start_application_window_capture(&self, window_id: u32) -> bool {
        log::info!("🎯 Starting ScreenCaptureKit window-specific capture for window ID: {}", window_id);
        let success = unsafe { syphon_server_start_application_window_capture(window_id) };
        if success {
            log::info!("✅ Window-specific capture initialized successfully for window {}", window_id);
        } else {
            log::error!("❌ Failed to initialize window-specific capture for window {}", window_id);
        }
        success
    }

    /// PHASE E.3: Start content-only capture using ScreenCaptureKit (excludes decorations)
    #[cfg(target_os = "macos")]
    pub fn start_content_only_window_capture(&self, window_id: u32) -> bool {
        log::info!("🎯 Phase E.3: Starting ScreenCaptureKit content-only capture for window ID: {}", window_id);
        let success = unsafe { syphon_server_start_content_only_window_capture(window_id) };
        if success {
            log::info!("✅ Phase E.3: Content-only capture initialized successfully for window {} (no decorations)", window_id);
        } else {
            log::error!("❌ Phase E.3: Failed to initialize content-only capture for window {}", window_id);
        }
        success
    }

    /// REAL SCREEN CAPTURE: Publish latest captured screen frame via zero-copy
    #[cfg(target_os = "macos")]
    pub fn publish_screen_capture(&self) -> bool {
        if let Ok(server_guard) = self.server_state.lock() {
            if let Some(server_state) = *server_guard {
                let success = unsafe {
                    syphon_server_publish_screen_capture(server_state)
                };

                if success {
                    let count = self.frame_count.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                    if count % 60 == 0 { // Log every 60 frames (1 second at 60fps)
                        log::info!("🎬 REAL SCREEN CAPTURE: Published frame {} (zero-copy)", count);
                    }
                } else {
                    log::debug!("📋 No screen frame available (normal when no new frames)");
                }

                success
            } else {
                log::error!("❌ Syphon server state not initialized for screen capture");
                false
            }
        } else {
            log::error!("❌ Failed to lock Syphon server state for screen capture");
            false
        }
    }

    /// REAL SCREEN CAPTURE: Check if new screen frames are available
    #[cfg(target_os = "macos")]
    pub fn has_screen_frames(&self) -> bool {
        unsafe { syphon_server_has_screen_frame() }
    }

    /// REAL SCREEN CAPTURE: Stop screen capture system
    #[cfg(target_os = "macos")]
    pub fn stop_screen_capture(&self) {
        log::info!("🛑 Stopping real screen capture");
        unsafe { syphon_server_stop_screen_capture() };
        log::info!("✅ Screen capture stopped");
    }

    /// PHASE D: Test simple data bridge - get frontmost window ID
    #[cfg(target_os = "macos")]
    pub fn get_frontmost_window_id(&self) -> u32 {
        unsafe { get_frontmost_window_id() }
    }
    
    /// PHASE E: Get our own application's main window information
    #[cfg(target_os = "macos")]
    pub fn get_our_window_info(&self) -> u32 {
        unsafe { get_our_window_info() }
    }
    
    /// PHASE G: Get window by title prefix (for content window targeting)
    #[cfg(target_os = "macos")]
    pub fn get_window_by_title_prefix(&self, title_prefix: &str) -> u32 {
        let c_title = CString::new(title_prefix).unwrap_or_else(|_| CString::new("").unwrap());
        unsafe { get_window_by_title_prefix(c_title.as_ptr()) }
    }

    

    /// Legacy bitmap-based frame publishing (fallback method)
    pub fn update_frame(&self, data: &[u8], width: u32, height: u32) -> bool {
        #[cfg(target_os = "macos")]
        {
            if let Ok(server_guard) = self.server_state.lock() {
                if let Some(server_state) = *server_guard {
                    let success = unsafe {
                        syphon_server_publish_frame(
                            server_state,
                            data.as_ptr(),
                            width,
                            height,
                        )
                    };

                    if success {
                        let count = self.frame_count.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                        if count % 30 == 0 { // Log every 30 frames to avoid spam
                            log::info!("Published Syphon frame {}: {}x{} ({} bytes)", count, width, height, data.len());
                        }
                    } else {
                        log::warn!("Failed to publish Syphon frame: {}x{}", width, height);
                    }

                    success
                } else {
                    log::error!("Syphon server state not initialized");
                    false
                }
            } else {
                log::error!("Failed to lock Syphon server state");
                false
            }
        }

        #[cfg(not(target_os = "macos"))]
        {
            log::warn!("Syphon not available on this platform");
            false
        }
    }

    pub fn stop(&self) {
        #[cfg(target_os = "macos")]
        {
            log::info!("🛑 Stopping SyphonOutput: {}", self.name);
            
            // Stop screen capture first
            self.stop_screen_capture();
            
            if let Ok(mut server_guard) = self.server_state.lock() {
                if let Some(server_state) = server_guard.take() {
                    unsafe {
                        syphon_server_stop(server_state);
                    }
                    log::info!("✅ Syphon server stopped and cleaned up");
                }
            }
            
            // Clean up the dedicated OpenGL context
            if let Some(context) = self.gl_context {
                unsafe {
                    let _: () = msg_send![context, release];
                }
                log::info!("✅ Dedicated NSOpenGLContext released");
            }
        }
    }

    pub fn get_frame_count(&self) -> u64 {
        self.frame_count.load(std::sync::atomic::Ordering::Relaxed)
    }

    pub fn has_clients(&self) -> bool {
        #[cfg(target_os = "macos")]
        {
            if let Ok(server_guard) = self.server_state.lock() {
                if let Some(server_state) = *server_guard {
                    unsafe { syphon_server_has_clients(server_state) }
                } else {
                    false
                }
            } else {
                false
            }
        }

        #[cfg(not(target_os = "macos"))]
        {
            false
        }
    }
}

impl Drop for SyphonOutput {
    fn drop(&mut self) {
        self.stop();
    }
}