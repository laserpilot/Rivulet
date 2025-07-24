// Tauri application for video sharing via Syphon/Spout
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use anyhow::Result;
use log::{debug, error, info, warn};
use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};
use tauri::{command, generate_context, generate_handler, Builder, Manager, State, Window};

// Removed raw_window_handle imports - no longer needed with application-based capture

// Import our high-performance implementations
#[cfg(target_os = "macos")]
mod syphon_simple;
#[cfg(target_os = "macos")]
use syphon_simple::SyphonOutput;

// TODO: Windows Spout implementation
#[cfg(target_os = "windows")]
struct SpoutOutput {
    name: String,
}

#[cfg(target_os = "windows")]
impl SpoutOutput {
    fn new(name: String) -> Self {
        Self { name }
    }
    
    fn update_frame(&self, _data: &[u8], _width: u32, _height: u32) -> bool {
        false // Placeholder
    }
    
    fn stop(&self) {}
}

/// Application state for high-performance video sharing
pub struct VideoShareState {
    #[cfg(target_os = "macos")]
    video_output: Option<SyphonOutput>,
    #[cfg(target_os = "windows")]
    video_output: Option<SpoutOutput>,
    frame_count: u64,
    is_screen_capture_active: bool,
}

impl VideoShareState {
    fn new() -> Self {
        Self {
            #[cfg(target_os = "macos")]
            video_output: None,
            #[cfg(target_os = "windows")]
            video_output: None,
            frame_count: 0,
            is_screen_capture_active: false,
        }
    }

    fn initialize_video_output(&mut self) -> Result<()> {
        info!("🚀 Initializing high-performance video output");
        
        #[cfg(target_os = "macos")]
        {
            // Initialize Syphon output
            self.video_output = Some(SyphonOutput::new("Tauri Video Share - Screen Capture".to_string()));
            info!("✅ Syphon output initialized");
        }
        
        #[cfg(target_os = "windows")]
        {
            self.video_output = Some(SpoutOutput::new("Tauri Video Share".to_string()));
            info!("Spout output initialized");
        }
        
        Ok(())
    }

    /// Initialize real screen capture pipeline (macOS)
    #[cfg(target_os = "macos")]
    fn initialize_screen_capture(&mut self) -> Result<()> {
        info!("🎬 Initializing real AVFoundation screen capture pipeline");
        
        if let Some(video_output) = &self.video_output {
            // Initialize the screen capture system (desktop capture for now)
            if video_output.start_screen_capture() {
                self.is_screen_capture_active = true;
                info!("✅ Real screen capture pipeline active");
                Ok(())
            } else {
                Err(anyhow::anyhow!("Failed to start screen capture"))
            }
        } else {
            Err(anyhow::anyhow!("Video output not initialized"))
        }
    }

    /// Initialize application-based screen capture pipeline (macOS) - PREFERRED
    #[cfg(target_os = "macos")]
    fn initialize_application_capture(&mut self) -> Result<()> {
        info!("🎯 Initializing ScreenCaptureKit application-based capture");
        
        if let Some(video_output) = &self.video_output {
            // Initialize the application capture system
            if video_output.start_application_capture() {
                self.is_screen_capture_active = true;
                info!("✅ Application capture pipeline active");
                Ok(())
            } else {
                Err(anyhow::anyhow!("Failed to start application capture"))
            }
        } else {
            Err(anyhow::anyhow!("Video output not initialized"))
        }
    }

    /// PHASE E.2: Initialize window-specific screen capture pipeline (macOS)
    #[cfg(target_os = "macos")]
    fn initialize_application_window_capture(&mut self, window_id: u32) -> Result<()> {
        info!("🎯 Initializing ScreenCaptureKit window-specific capture for window ID: {}", window_id);
        
        if let Some(video_output) = &self.video_output {
            // Initialize the window-specific capture system
            if video_output.start_application_window_capture(window_id) {
                self.is_screen_capture_active = true;
                info!("✅ Window-specific capture pipeline active for window {}", window_id);
                Ok(())
            } else {
                Err(anyhow::anyhow!("Failed to start window-specific capture for window {}", window_id))
            }
        } else {
            Err(anyhow::anyhow!("Video output not initialized"))
        }
    }

    /// PHASE E.3: Initialize content-only screen capture pipeline (macOS)
    #[cfg(target_os = "macos")]
    fn initialize_content_only_window_capture(&mut self, window_id: u32) -> Result<()> {
        info!("🎯 Phase E.3: Initializing ScreenCaptureKit content-only capture for window ID: {}", window_id);
        
        if let Some(video_output) = &self.video_output {
            // Initialize the content-only capture system
            if video_output.start_content_only_window_capture(window_id) {
                self.is_screen_capture_active = true;
                info!("✅ Phase E.3: Content-only capture pipeline active for window {} (no decorations)", window_id);
                Ok(())
            } else {
                Err(anyhow::anyhow!("Phase E.3: Failed to start content-only capture for window {}", window_id))
            }
        } else {
            Err(anyhow::anyhow!("Video output not initialized"))
        }
    }

    /// Initialize window-specific screen capture pipeline (macOS) - DEPRECATED
    #[cfg(target_os = "macos")]
    fn initialize_window_capture(&mut self, window_id: u32) -> Result<()> {
        warn!("⚠️ initialize_window_capture is DEPRECATED - use initialize_application_capture instead");
        info!("🎯 Initializing window-specific AVFoundation capture for CGWindowID: {}", window_id);
        
        if let Some(video_output) = &self.video_output {
            // Initialize the window capture system
            if video_output.start_window_capture(window_id) {
                self.is_screen_capture_active = true;
                info!("✅ Window capture pipeline active for window {}", window_id);
                Ok(())
            } else {
                Err(anyhow::anyhow!("Failed to start window capture"))
            }
        } else {
            Err(anyhow::anyhow!("Video output not initialized"))
        }
    }

    /// Process real screen capture frames via zero-copy (macOS)
    #[cfg(target_os = "macos")]
    fn process_screen_capture_frames(&mut self) -> u32 {
        let mut frames_processed = 0;
        
        if self.is_screen_capture_active {
            if let Some(video_output) = &self.video_output {
                // Check if new frames are available
                if video_output.has_screen_frames() {
                    // Publish the latest screen capture frame
                    if video_output.publish_screen_capture() {
                        self.frame_count += 1;
                        frames_processed = 1;
                    } else {
                        log::debug!("📋 No new screen frame to publish (normal behavior)");
                    }
                }
            }
        }
        
        frames_processed
    }

    fn update_frame(&mut self, data: &[u8], width: u32, height: u32) -> bool {
        if let Some(ref video_output) = self.video_output {
            let success = video_output.update_frame(data, width, height);
            if success {
                self.frame_count += 1;
                if self.frame_count % 30 == 0 { // Log every 30 frames
                    debug!("Published frame {}: {}x{} ({} bytes)", 
                           self.frame_count, width, height, data.len());
                }
            } else {
                error!("Failed to publish frame");
            }
            success
        } else {
            warn!("Video output not initialized");
            false
        }
    }

    /// TRUE ZERO-COPY: Publish IOSurface frame (macOS only)
    #[cfg(target_os = "macos")]
    fn publish_zero_copy_frame(&mut self, width: u32, height: u32) -> bool {
        if let Some(ref video_output) = self.video_output {
            let success = video_output.publish_test_iosurface(width, height);
            if success {
                self.frame_count += 1;
            } else {
                error!("Failed to publish zero-copy frame");
            }
            success
        } else {
            warn!("Video output not initialized");
            false
        }
    }

    /// Stop screen capture and cleanup
    #[cfg(target_os = "macos")]
    fn stop_screen_capture(&mut self) {
        if self.is_screen_capture_active {
            if let Some(video_output) = &self.video_output {
                video_output.stop_screen_capture();
            }
            self.is_screen_capture_active = false;
            info!("🛑 Screen capture stopped");
        }
    }
}

/// Request/response types for Tauri commands
#[derive(Serialize, Deserialize)]
pub struct FrameData {
    pub data: Vec<u8>,
    pub width: u32,
    pub height: u32,
}

#[derive(Serialize, Deserialize)]
pub struct VideoResponse {
    pub success: bool,
    pub message: String,
    pub frame_count: u64,
}

/// Tauri command to initialize video sharing
#[command]
fn initialize_video_sharing(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("Initializing video sharing from frontend");
    
    let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
    
    match video_state.initialize_video_output() {
        Ok(()) => {
            info!("Video sharing initialized successfully");
            Ok(VideoResponse {
                success: true,
                message: "Video sharing initialized".to_string(),
                frame_count: video_state.frame_count,
            })
        }
        Err(e) => {
            error!("Failed to initialize video sharing: {}", e);
            Err(format!("Initialization failed: {}", e))
        }
    }
}

/// Tauri command to publish a frame
#[command]
fn publish_frame(
    frame: FrameData,
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    debug!("Received frame from frontend: {}x{} ({} bytes)", 
           frame.width, frame.height, frame.data.len());
    
    let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
    
    let success = video_state.update_frame(&frame.data, frame.width, frame.height);
    
    Ok(VideoResponse {
        success,
        message: if success { "Frame published".to_string() } else { "Failed to publish frame".to_string() },
        frame_count: video_state.frame_count,
    })
}

// Removed get_window_id_from_tauri() - no longer needed with application-based capture

/// Tauri command to initialize window self-capture (macOS) - DEPRECATED
#[command]
fn initialize_window_capture(
    _window: Window,
    _state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    warn!("⚠️ initialize_window_capture is DEPRECATED - use initialize_application_capture instead");
    Err("Window capture is deprecated. Use application capture instead.".to_string())
}

/// Tauri command to initialize application self-capture using ScreenCaptureKit (macOS) - PREFERRED
#[command]
fn initialize_application_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎯 Frontend requested application self-capture initialization");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        match video_state.initialize_application_capture() {
            Ok(()) => {
                info!("✅ Application self-capture initialized successfully");
                Ok(VideoResponse {
                    success: true,
                    message: "Application self-capture active (ScreenCaptureKit + zero-copy IOSurface)".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
            Err(e) => {
                error!("❌ Failed to initialize application capture: {}", e);
                Err(format!("Application capture initialization failed: {}", e))
            }
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Application capture only available on macOS".to_string())
    }
}

/// Tauri command to initialize real screen capture (macOS)
#[command]
fn initialize_screen_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎬 Frontend requested real screen capture initialization");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        match video_state.initialize_screen_capture() {
            Ok(()) => {
                info!("✅ Real screen capture initialized successfully");
                Ok(VideoResponse {
                    success: true,
                    message: "Real screen capture active (zero-copy IOSurface)".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
            Err(e) => {
                error!("❌ Failed to initialize screen capture: {}", e);
                Err(format!("Screen capture initialization failed: {}", e))
            }
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Screen capture only available on macOS".to_string())
    }
}

/// Tauri command to get video sharing status
#[command]
fn get_video_status(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    let video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
    
    let initialized = video_state.video_output.is_some();
    let mode = if video_state.is_screen_capture_active { "Real Screen Capture (Zero-Copy)" } else { "Legacy" };
    
    Ok(VideoResponse {
        success: initialized,
        message: format!("Video sharing active ({})", mode),
        frame_count: video_state.frame_count,
    })
}

/// Tauri command to process real screen capture frames (macOS)
#[command]
fn process_frames(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        if video_state.is_screen_capture_active {
            let frames_processed = video_state.process_screen_capture_frames();
            
            Ok(VideoResponse {
                success: true,
                message: format!("Processed {} screen capture frames (zero-copy)", frames_processed),
                frame_count: video_state.frame_count,
            })
        } else {
            Ok(VideoResponse {
                success: false,
                message: "Screen capture mode not active".to_string(),
                frame_count: video_state.frame_count,
            })
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Screen capture processing only available on macOS".to_string())
    }
}

/// PRODUCTION: Start continuous real screen capture publishing (macOS)
#[command]
fn start_continuous_screen_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🚀 Frontend requested continuous real screen capture");
    
    #[cfg(target_os = "macos")]
    {
        let video_state = Arc::clone(state.inner());
        
        // Check if already initialized
        {
            let mut vs = video_state.lock().map_err(|e| format!("Failed to lock state: {}", e))?;
            if !vs.is_screen_capture_active {
                return Err("Screen capture not initialized. Call initialize_screen_capture first.".to_string());
            }
        }
        
        // Spawn background thread for continuous publishing
        let state_clone = Arc::clone(&video_state);
        std::thread::spawn(move || {
            info!("🎬 Starting continuous screen capture publishing thread");
            
            loop {
                let should_continue = {
                    if let Ok(mut vs) = state_clone.lock() {
                        if !vs.is_screen_capture_active {
                            info!("🛑 Screen capture no longer active, stopping publishing thread");
                            break;
                        }
                        
                        // Process screen capture frames continuously
                        vs.process_screen_capture_frames();
                        true
                    } else {
                        warn!("Failed to lock state in publishing thread");
                        false
                    }
                };
                
                if !should_continue {
                    break;
                }
                
                // Target ~60 FPS (16.67ms per frame)
                std::thread::sleep(std::time::Duration::from_millis(16));
            }
            
            info!("🏁 Continuous screen capture publishing thread ended");
        });
        
        Ok(VideoResponse {
            success: true,
            message: "Continuous real screen capture started (60 FPS zero-copy)".to_string(),
            frame_count: 0,
        })
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Continuous screen capture only available on macOS".to_string())
    }
}

/// PRODUCTION: Stop continuous screen capture (macOS)
#[command]
fn stop_screen_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🛑 Frontend requested screen capture stop");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        video_state.stop_screen_capture();
        
        Ok(VideoResponse {
            success: true,
            message: "Screen capture stopped".to_string(),
            frame_count: video_state.frame_count,
        })
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Screen capture stop only available on macOS".to_string())
    }
}

/// TRUE ZERO-COPY: Test IOSurface publishing (macOS) - Legacy test method
#[command]
fn test_zero_copy_iosurface(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎯 Frontend requested TRUE zero-copy IOSurface test (legacy mock)");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        // Publish test IOSurface frame (1920x1080 for high resolution test)
        let success = video_state.publish_zero_copy_frame(1920, 1080);
        
        if success {
            info!("✅ TRUE ZERO-COPY: Mock IOSurface published successfully");
            Ok(VideoResponse {
                success: true,
                message: "TRUE ZERO-COPY Mock IOSurface published (use real screen capture for production)".to_string(),
                frame_count: video_state.frame_count,
            })
        } else {
            error!("❌ Failed to publish TRUE zero-copy mock IOSurface");
            Err("Failed to publish zero-copy mock IOSurface".to_string())
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("True zero-copy IOSurface only available on macOS".to_string())
    }
}

/// PHASE D: Test simple data bridge - get frontmost window ID
#[command]
fn test_get_frontmost_window_id(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🔍 Frontend requested frontmost window ID test (Phase D)");
    
    #[cfg(target_os = "macos")]
    {
        let video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        if let Some(video_output) = &video_state.video_output {
            let window_id = video_output.get_frontmost_window_id();
            
            if window_id != 0 {
                info!("✅ PHASE D SUCCESS: Found frontmost window ID: {}", window_id);
                Ok(VideoResponse {
                    success: true,
                    message: format!("PHASE D Success: Found frontmost window ID: {}", window_id),
                    frame_count: video_state.frame_count,
                })
            } else {
                warn!("⚠️ PHASE D: No valid frontmost window found");
                Ok(VideoResponse {
                    success: false,
                    message: "PHASE D: No valid frontmost window found (try opening another application window)".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
        } else {
            Err("Video output not initialized. Call initialize_video_sharing first.".to_string())
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Window ID detection only available on macOS".to_string())
    }
}

/// PHASE E: Test getting our own application's window information
#[command]
fn test_get_our_window_info(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎯 Frontend requested our window info test (Phase E)");
    
    #[cfg(target_os = "macos")]
    {
        let video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        if let Some(video_output) = &video_state.video_output {
            let window_id = video_output.get_our_window_info();
            
            if window_id != 0 {
                info!("✅ PHASE E SUCCESS: Found our window ID: {}", window_id);
                Ok(VideoResponse {
                    success: true,
                    message: format!("PHASE E Success: Found our window ID: {}", window_id),
                    frame_count: video_state.frame_count,
                })
            } else {
                warn!("⚠️ PHASE E: No valid window found for our application");
                Ok(VideoResponse {
                    success: false,
                    message: "PHASE E: No valid window found for our application".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
        } else {
            Err("Video output not initialized. Call initialize_video_sharing first.".to_string())
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Window info detection only available on macOS".to_string())
    }
}

/// PHASE E.2: Test window-specific capture initialization
#[command]
fn test_initialize_window_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎯 Frontend requested window-specific capture test (Phase E.2)");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        if let Some(video_output) = &video_state.video_output {
            // First, get our window ID
            let window_id = video_output.get_our_window_info();
            
            if window_id != 0 {
                info!("🎯 Found our window ID: {}, initializing window-specific capture...", window_id);
                
                // Initialize window-specific capture
                match video_state.initialize_application_window_capture(window_id) {
                    Ok(()) => {
                        info!("✅ PHASE E.2 SUCCESS: Window-specific capture initialized for window {}", window_id);
                        Ok(VideoResponse {
                            success: true,
                            message: format!("PHASE E.2 Success: Window-specific capture initialized for window {}", window_id),
                            frame_count: video_state.frame_count,
                        })
                    }
                    Err(e) => {
                        error!("❌ PHASE E.2 FAILED: {}", e);
                        Err(format!("PHASE E.2 window capture failed: {}", e))
                    }
                }
            } else {
                warn!("⚠️ PHASE E.2: Cannot find our window ID for capture");
                Ok(VideoResponse {
                    success: false,
                    message: "PHASE E.2: Cannot find our window ID for capture".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
        } else {
            Err("Video output not initialized. Call initialize_video_sharing first.".to_string())
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Window-specific capture only available on macOS".to_string())
    }
}

/// Toggle window decorations (title bar, borders, etc.)
#[command]
fn toggle_window_decorations(window: Window) -> Result<VideoResponse, String> {
    info!("🎨 Frontend requested window decoration toggle");
    
    // Get current decorations state - use window manager API
    let app_handle = window.app_handle();
    let main_window = app_handle.get_window("main").ok_or("Main window not found")?;
    
    // For Tauri v1, we need to use the set_decorations method
    // Since we can't easily get current state, we'll toggle based on a static variable
    static mut DECORATIONS_ENABLED: bool = true;
    
    unsafe {
        DECORATIONS_ENABLED = !DECORATIONS_ENABLED;
        main_window.set_decorations(DECORATIONS_ENABLED)
            .map_err(|e| format!("Failed to set decorations: {}", e))?;
        
        let status = if DECORATIONS_ENABLED { "enabled" } else { "disabled" };
        info!("✅ Window decorations {}", status);
        
        Ok(VideoResponse {
            success: true,
            message: format!("Window decorations {} (title bar {})", 
                           status, 
                           if DECORATIONS_ENABLED { "visible" } else { "hidden" }),
            frame_count: 0,
        })
    }
}

/// Set window to frameless mode (no decorations at all)
#[command]
fn set_frameless_mode(window: Window, frameless: bool) -> Result<VideoResponse, String> {
    info!("🖼️ Frontend requested frameless mode: {}", frameless);
    
    let app_handle = window.app_handle();
    let main_window = app_handle.get_window("main").ok_or("Main window not found")?;
    
    // Set decorations (false = frameless)
    main_window.set_decorations(!frameless)
        .map_err(|e| format!("Failed to set frameless mode: {}", e))?;
    
    let mode = if frameless { "frameless" } else { "normal" };
    info!("✅ Window set to {} mode", mode);
    
    Ok(VideoResponse {
        success: true,
        message: format!("Window mode: {} (decorations {})", 
                        mode, 
                        if frameless { "hidden" } else { "visible" }),
        frame_count: 0,
    })
}

/// PHASE E.3: Test content-only window capture (excludes decorations)
#[command]
fn test_initialize_content_only_capture(
    state: State<Arc<Mutex<VideoShareState>>>,
) -> Result<VideoResponse, String> {
    info!("🎯 Frontend requested content-only capture test (Phase E.3)");
    
    #[cfg(target_os = "macos")]
    {
        let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
        
        if let Some(video_output) = &video_state.video_output {
            // First, get our window ID
            let window_id = video_output.get_our_window_info();
            
            if window_id != 0 {
                info!("🎯 Phase E.3: Found our window ID: {}, initializing content-only capture...", window_id);
                
                // Initialize content-only capture
                match video_state.initialize_content_only_window_capture(window_id) {
                    Ok(()) => {
                        info!("✅ PHASE E.3 SUCCESS: Content-only capture initialized for window {} (no decorations)", window_id);
                        
                        // Start continuous publishing for Phase E.3
                        let state_clone = Arc::clone(&state.inner());
                        std::thread::spawn(move || {
                            info!("🎬 Phase E.3: Starting continuous content-only publishing thread");
                            
                            loop {
                                let should_continue = {
                                    if let Ok(mut vs) = state_clone.lock() {
                                        if !vs.is_screen_capture_active {
                                            info!("🛑 Phase E.3: Content-only capture no longer active, stopping publishing thread");
                                            break;
                                        }
                                        
                                        // Process captured frames continuously
                                        vs.process_screen_capture_frames();
                                        true
                                    } else {
                                        warn!("Phase E.3: Failed to lock state in publishing thread");
                                        false
                                    }
                                };
                                
                                if !should_continue {
                                    break;
                                }
                                
                                // Target ~60 FPS (16.67ms per frame)
                                std::thread::sleep(std::time::Duration::from_millis(16));
                            }
                            
                            info!("🏁 Phase E.3: Content-only publishing thread ended");
                        });
                        
                        Ok(VideoResponse {
                            success: true,
                            message: format!("PHASE E.3 Success: Content-only capture with publishing started for window {} (excludes decorations)", window_id),
                            frame_count: video_state.frame_count,
                        })
                    }
                    Err(e) => {
                        error!("❌ PHASE E.3 FAILED: {}", e);
                        Err(format!("PHASE E.3 content-only capture failed: {}", e))
                    }
                }
            } else {
                warn!("⚠️ PHASE E.3: Cannot find our window ID for content-only capture");
                Ok(VideoResponse {
                    success: false,
                    message: "PHASE E.3: Cannot find our window ID for content-only capture".to_string(),
                    frame_count: video_state.frame_count,
                })
            }
        } else {
            Err("Video output not initialized. Call initialize_video_sharing first.".to_string())
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Content-only capture only available on macOS".to_string())
    }
}

/// PHASE F: Create content window for clean web streaming
#[command]
fn create_content_window(app: tauri::AppHandle) -> Result<VideoResponse, String> {
    info!("🌐 Creating content window for clean web streaming");
    
    let window_builder = tauri::WindowBuilder::new(
        &app,
        "content-window",
        tauri::WindowUrl::App("content-window.html".into())
    )
    .title("Web Content - www.ablairneal.com")
    .inner_size(1200.0, 800.0)
    .resizable(true)
    .decorations(false) // Start frameless for clean capture
    .always_on_top(false)
    .focused(true);
    
    match window_builder.build() {
        Ok(_window) => {
            info!("✅ Content window created successfully");
            Ok(VideoResponse {
                success: true,
                message: "Content window created for clean web streaming".to_string(),
                frame_count: 0,
            })
        }
        Err(e) => {
            error!("❌ Failed to create content window: {}", e);
            Err(format!("Failed to create content window: {}", e))
        }
    }
}

/// PHASE G: Create content window with dynamic size and URL
#[command]
fn create_content_window_dynamic(
    app: tauri::AppHandle, 
    width: f64, 
    height: f64, 
    url: String
) -> Result<VideoResponse, String> {
    info!("🌐 Creating dynamic content window: {}x{} → {}", width, height, url);
    
    // Validate parameters
    if width < 800.0 || width > 3840.0 {
        return Err("Width must be between 800 and 3840 pixels".to_string());
    }
    if height < 600.0 || height > 2160.0 {
        return Err("Height must be between 600 and 2160 pixels".to_string());
    }
    if !url.starts_with("https://") {
        return Err("URL must start with https://".to_string());
    }
    
    // Extract domain for window title
    let domain = url.split("://")
        .nth(1)
        .unwrap_or("unknown")
        .split("/")
        .next()
        .unwrap_or("unknown");
    
    let window_builder = tauri::WindowBuilder::new(
        &app,
        "content-window",
        tauri::WindowUrl::App("content-window.html".into())
    )
    .title(&format!("Web Content - {} ({}x{})", domain, width as i32, height as i32))
    .inner_size(width, height)
    .resizable(true)
    .decorations(true) // Start with decorations so user can move the window
    .always_on_top(false)
    .focused(true);
    
    match window_builder.build() {
        Ok(_window) => {
            info!("✅ Dynamic content window created successfully: {}x{}", width, height);
            Ok(VideoResponse {
                success: true,
                message: format!("Content window created ({}x{}) for {}", 
                    width as i32, height as i32, domain),
                frame_count: 0,
            })
        }
        Err(e) => {
            error!("❌ Failed to create dynamic content window: {}", e);
            Err(format!("Failed to create content window: {}", e))
        }
    }
}

/// PHASE G: Update content window URL after creation
#[command]
fn update_content_url(app: tauri::AppHandle, url: String) -> Result<VideoResponse, String> {
    info!("🔄 Updating content window URL to: {}", url);
    
    // Validate URL
    if !url.starts_with("https://") {
        return Err("URL must start with https://".to_string());
    }
    
    // Get the content window
    match app.get_window("content-window") {
        Some(window) => {
            // Emit event to content window to update URL
            match window.emit("update-url", &url) {
                Ok(_) => {
                    info!("✅ URL update event sent to content window");
                    Ok(VideoResponse {
                        success: true,
                        message: format!("Content URL updated to {}", url),
                        frame_count: 0,
                    })
                }
                Err(e) => {
                    error!("❌ Failed to emit URL update event: {}", e);
                    Err(format!("Failed to update URL: {}", e))
                }
            }
        }
        None => {
            error!("❌ Content window not found");
            Err("Content window not found. Please create content window first.".to_string())
        }
    }
}

/// PHASE G: Set content window decorations (title bar)
#[command]
fn set_content_window_decorations(app: tauri::AppHandle, show_decorations: bool) -> Result<VideoResponse, String> {
    info!("🎨 Setting content window decorations: {}", show_decorations);
    
    match app.get_window("content-window") {
        Some(window) => {
            match window.set_decorations(show_decorations) {
                Ok(_) => {
                    let message = if show_decorations {
                        "Content window title bar shown - you can now move the window"
                    } else {
                        "Content window title bar hidden - clean for capture"
                    };
                    
                    info!("✅ Content window decorations set to: {}", show_decorations);
                    Ok(VideoResponse {
                        success: true,
                        message: message.to_string(),
                        frame_count: 0,
                    })
                }
                Err(e) => {
                    error!("❌ Failed to set content window decorations: {}", e);
                    Err(format!("Failed to set decorations: {}", e))
                }
            }
        }
        None => {
            error!("❌ Content window not found");
            Err("Content window not found. Please create content window first.".to_string())
        }
    }
}

/// PHASE G: Initialize content window specific capture
#[command]
fn start_content_window_streaming(
    state: State<Arc<Mutex<VideoShareState>>>,
    app: tauri::AppHandle,
) -> Result<VideoResponse, String> {
    info!("🎯 Starting content window specific streaming");
    
    #[cfg(target_os = "macos")]
    {
        // First, ensure we have the content window
        match app.get_window("content-window") {
            Some(content_window) => {
                let mut video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
                
                if let Some(video_output) = &video_state.video_output {
                    // Get the content window ID by searching for "Web Content" window
                    let window_id = video_output.get_window_by_title_prefix("Web Content");
                    
                    if window_id != 0 {
                        info!("🎯 Found content window ID: {}, initializing capture...", window_id);
                        
                        // Initialize content-only capture for the content window
                        match video_state.initialize_content_only_window_capture(window_id) {
                            Ok(()) => {
                                info!("✅ Content window capture initialized for window {}", window_id);
                                
                                // Content window capture is now active, but don't start background thread
                                // OpenGL operations must happen on the main thread where context was created
                                info!("✅ Content window capture ready - use process_frames command to publish frames");
                                
                                video_state.is_screen_capture_active = true;
                                
                                Ok(VideoResponse {
                                    success: true,
                                    message: format!("Content window streaming active (Window ID: {})", window_id),
                                    frame_count: 0,
                                })
                            }
                            Err(e) => {
                                error!("❌ Failed to initialize content window capture: {}", e);
                                Err(format!("Failed to initialize content window capture: {}", e))
                            }
                        }
                    } else {
                        error!("❌ Content window not found or invalid");
                        Err("Content window not found. Please create content window first.".to_string())
                    }
                } else {
                    error!("❌ Video output not initialized");
                    Err("Syphon server not initialized. Please initialize first.".to_string())
                }
            }
            None => {
                error!("❌ Content window not found");
                Err("Content window not found. Please create content window first.".to_string())
            }
        }
    }
    
    #[cfg(not(target_os = "macos"))]
    {
        Err("Content window streaming only supported on macOS".to_string())
    }
}

/// Close content window with proper cleanup
#[command]
async fn close_content_window(app: tauri::AppHandle) -> Result<VideoResponse, String> {
    info!("🔒 Closing content window...");
    
    match app.get_window("content-window") {
        Some(content_window) => {
            // Close the window
            if let Err(e) = content_window.close() {
                error!("❌ Failed to close content window: {}", e);
                return Err(format!("Failed to close content window: {}", e));
            }
            
            info!("✅ Content window closed successfully");
            Ok(VideoResponse {
                success: true,
                message: "Content window closed successfully".to_string(),
                frame_count: 0,
            })
        }
        None => {
            warn!("⚠️ Content window not found - already closed?");
            Ok(VideoResponse {
                success: true,
                message: "Content window not found (already closed)".to_string(),
                frame_count: 0,
            })
        }
    }
}

/// Get current application state for frontend synchronization
#[command]
async fn get_application_state(state: State<'_, Arc<Mutex<VideoShareState>>>, app: tauri::AppHandle) -> Result<ApplicationStateResponse, String> {
    info!("🔄 Getting application state...");
    
    let video_state = state.inner().lock().map_err(|e| format!("Failed to lock state: {}", e))?;
    
    let initialized = video_state.video_output.is_some();
    let capturing = video_state.is_screen_capture_active;
    let content_window_exists = app.get_window("content-window").is_some();
    
    Ok(ApplicationStateResponse {
        success: true,
        message: "Application state retrieved".to_string(),
        state: ApplicationState {
            initialized,
            capturing,
            content_window_exists,
            frame_count: video_state.frame_count,
        }
    })
}

/// Reload content in existing content window
#[command]
async fn reload_content_window(app: tauri::AppHandle) -> Result<VideoResponse, String> {
    info!("🔄 Reloading content window...");
    
    match app.get_window("content-window") {
        Some(content_window) => {
            // Emit reload event to the content window
            if let Err(e) = content_window.emit("reload-content", ()) {
                error!("❌ Failed to emit reload event: {}", e);
                return Err(format!("Failed to reload content window: {}", e));
            }
            
            info!("✅ Content window reload triggered");
            Ok(VideoResponse {
                success: true,
                message: "Content window reload triggered".to_string(),
                frame_count: 0,
            })
        }
        None => {
            error!("❌ Content window not found");
            Err("Content window not found. Please create content window first.".to_string())
        }
    }
}

// Additional response types for new commands
#[derive(Serialize)]
struct ApplicationStateResponse {
    success: bool,
    message: String,
    state: ApplicationState,
}

#[derive(Serialize)]
struct ApplicationState {
    initialized: bool,
    capturing: bool,
    content_window_exists: bool,
    frame_count: u64,
}

fn main() {
    // Initialize logging
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Info)
        .init();
    
    info!("Starting Tauri Video Share Application");
    info!("Platform: {}", std::env::consts::OS);
    
    // Create application state
    let video_state = Arc::new(Mutex::new(VideoShareState::new()));
    
    // Build and run Tauri application
    Builder::default()
        .manage(video_state)
        .invoke_handler(generate_handler![
            initialize_video_sharing,
            publish_frame,
            get_video_status,
            initialize_screen_capture,
            initialize_window_capture,      // DEPRECATED
            initialize_application_capture, // NEW: Application-based capture
            process_frames,
            start_continuous_screen_capture,
            stop_screen_capture,
            test_zero_copy_iosurface,
            test_get_frontmost_window_id,   // PHASE D: Test data bridge
            test_get_our_window_info,       // PHASE E: Get our window info
            test_initialize_window_capture, // PHASE E.2: Test window-specific capture
            test_initialize_content_only_capture,  // PHASE E.3: Test content-only capture
            toggle_window_decorations,      // Toggle title bar visibility
            set_frameless_mode,             // Set frameless/normal mode
            create_content_window,          // PHASE F: Create content window
            create_content_window_dynamic,  // PHASE G: Create dynamic content window
            update_content_url,             // PHASE G: Update content window URL
            set_content_window_decorations, // PHASE G: Toggle content window title bar
            start_content_window_streaming, // PHASE G: Start content window specific streaming
            close_content_window,           // NEW: Close content window with cleanup
            get_application_state,          // NEW: Get application state for sync
            reload_content_window           // NEW: Reload content window
        ])
        .setup(|app| {
            info!("Tauri application setup complete");
            Ok(())
        })
        .run(generate_context!())
        .expect("Error while running Tauri application");
    
    info!("Tauri application shutdown complete");
}