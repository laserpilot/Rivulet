// Minimal Tauri application to test IPC pipeline first
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};
use tauri::{command, generate_context, generate_handler, Builder, State};

/// Minimal video sharing state for testing
pub struct VideoState {
    frame_count: u64,
    initialized: bool,
}

impl VideoState {
    fn new() -> Self {
        Self {
            frame_count: 0,
            initialized: false,
        }
    }
}

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

#[command]
fn initialize_video_sharing(state: State<Arc<Mutex<VideoState>>>) -> Result<VideoResponse, String> {
    println!("Initializing video sharing...");
    
    let mut video_state = state.lock().map_err(|e| format!("Lock error: {}", e))?;
    video_state.initialized = true;
    
    Ok(VideoResponse {
        success: true,
        message: "Video sharing initialized (testing mode)".to_string(),
        frame_count: video_state.frame_count,
    })
}

#[command]
fn publish_frame(
    frame: FrameData,
    state: State<Arc<Mutex<VideoState>>>,
) -> Result<VideoResponse, String> {
    let mut video_state = state.lock().map_err(|e| format!("Lock error: {}", e))?;
    
    if !video_state.initialized {
        return Err("Video sharing not initialized".to_string());
    }
    
    video_state.frame_count += 1;
    
    // Log every 30 frames to avoid spam
    if video_state.frame_count % 30 == 0 {
        println!("Received frame {}: {}x{} ({} bytes)", 
                video_state.frame_count, frame.width, frame.height, frame.data.len());
    }
    
    Ok(VideoResponse {
        success: true,
        message: "Frame received (testing mode)".to_string(),
        frame_count: video_state.frame_count,
    })
}

#[command]
fn get_video_status(state: State<Arc<Mutex<VideoState>>>) -> Result<VideoResponse, String> {
    let video_state = state.lock().map_err(|e| format!("Lock error: {}", e))?;
    
    Ok(VideoResponse {
        success: video_state.initialized,
        message: if video_state.initialized { 
            "Testing mode active".to_string() 
        } else { 
            "Not initialized".to_string() 
        },
        frame_count: video_state.frame_count,
    })
}

fn main() {
    println!("Starting Tauri Video Share (Testing Mode)");
    
    let video_state = Arc::new(Mutex::new(VideoState::new()));
    
    Builder::default()
        .manage(video_state)
        .invoke_handler(generate_handler![
            initialize_video_sharing,
            publish_frame,
            get_video_status
        ])
        .setup(|_app| {
            println!("Tauri application setup complete");
            Ok(())
        })
        .run(generate_context!())
        .expect("Error while running Tauri application");
}