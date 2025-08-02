use std::env;

fn main() {
    // Use tauri-build for Tauri configuration
    tauri_build::build();
    
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    
    // Setup platform-specific linking
    if target_os == "macos" {
        println!("cargo:warning=Setting up macOS Syphon framework linking");
        
        // Find the Syphon framework
        let syphon_framework_path = "../frameworks";
        
        // Compile Objective-C bridge and screen capture module
        cc::Build::new()
            .file("src/syphon_bridge.m")
            .file("src/screencapture_iosurface.m")  // Add screen capture implementation
            .flag("-fobjc-arc")
            .flag(&format!("-F{}", syphon_framework_path))  // Framework search path
            .flag("-framework")
            .flag("Syphon")
            .compile("syphon_bridge");
        
        // Link system frameworks (needed for objc crate, OpenGL, and screen capture)
        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=framework=AppKit");
        println!("cargo:rustc-link-lib=framework=OpenGL");
        println!("cargo:rustc-link-lib=framework=AVFoundation");     // For legacy screen capture
        println!("cargo:rustc-link-lib=framework=ScreenCaptureKit"); // For modern application capture
        println!("cargo:rustc-link-lib=framework=CoreMedia");        // For CMSampleBuffer
        println!("cargo:rustc-link-lib=framework=IOSurface");        // For IOSurface
        println!("cargo:rustc-link-lib=framework=CoreVideo");        // For CVPixelBuffer
        
        // Link Syphon framework
        println!("cargo:rustc-link-search=framework={}", syphon_framework_path);
        println!("cargo:rustc-link-lib=framework=Syphon");
        
        // Add rpath for Syphon framework
        if let Ok(absolute_framework_path) = std::fs::canonicalize(syphon_framework_path) {
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", absolute_framework_path.display());
        }
        
        // Tell cargo to recompile if Objective-C sources change
        println!("cargo:rerun-if-changed=src/syphon_bridge.m");
        println!("cargo:rerun-if-changed=src/screencapture_iosurface.m");
    }
    
    // Windows Spout setup
    if target_os == "windows" {
        println!("cargo:warning=Setting up Windows Spout C++ compilation");
        
        // Compile C++ Spout bridge and screen capture with real Spout2 SDK
        cc::Build::new()
            .cpp(true)
            .file("src/spout_bridge.cpp")
            .file("src/screencapture_d3d11.cpp") // Add screen capture implementation
            .include("lib/spout2/include") // Include real Spout2 SDK headers
            .flag("/std:c++17") // Use C++17 standard
            .flag("/EHsc") // Enable C++ exception handling
            .flag_if_supported("/permissive-") // Disable non-conforming code
            .compile("spout_bridge");
        
        // Link Windows system libraries required for D3D11 and DXGI
        println!("cargo:rustc-link-lib=d3d11");
        println!("cargo:rustc-link-lib=dxgi");
        println!("cargo:rustc-link-lib=dxguid");
        println!("cargo:rustc-link-lib=user32"); // For Win32 API
        println!("cargo:rustc-link-lib=kernel32"); // For Windows kernel functions
        
        // Link real Spout2 SDK
        println!("cargo:rustc-link-search=native=lib/spout2/lib/x64");
        println!("cargo:rustc-link-lib=SpoutLibrary"); // Real Spout2 library
        
        // Tell cargo to recompile if C++ sources change
        println!("cargo:rerun-if-changed=src/spout_bridge.cpp");
        println!("cargo:rerun-if-changed=src/screencapture_d3d11.cpp");
    }
}