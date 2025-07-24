use std::env;

fn main() {
    // Use tauri-build for Tauri configuration
    tauri_build::build();
    
    // Only setup additional linking on macOS
    if env::var("CARGO_CFG_TARGET_OS").unwrap() == "macos" {
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
}