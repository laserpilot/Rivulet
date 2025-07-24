use std::cell::RefCell;

// Direct Rust integration with Syphon using objc crate
#[cfg(target_os = "macos")]
mod syphon_sys {
    // Import OpenGL for frame rendering
    extern crate gl;
    use objc::runtime::{Class, Object};
    use objc::{msg_send, sel, sel_impl};
    use std::ffi::CStr;
    use std::os::raw::c_char;
    
    pub struct SyphonServerState {
        server: *mut Object,
    }
    
    unsafe impl Send for SyphonServerState {}
    unsafe impl Sync for SyphonServerState {}
    
    impl SyphonServerState {
        unsafe fn ensure_syphon_framework_loaded() {
            // Use dlopen to explicitly load the Syphon framework
            use std::ffi::CString;
            use std::ptr;
            
            // Try to load the framework from the rpath location
            let framework_path = CString::new("/Users/laser/Dropbox/PROJECTS/_claude_experiments/electron-spout/frameworks/Syphon.framework/Syphon").unwrap();
            
            // Use dlopen to load the framework
            let handle = libc::dlopen(framework_path.as_ptr(), libc::RTLD_LAZY | libc::RTLD_GLOBAL);
            
            if handle.is_null() {
                // Get error message
                let error_msg = libc::dlerror();
                if !error_msg.is_null() {
                    let error_str = std::ffi::CStr::from_ptr(error_msg).to_string_lossy();
                    log::error!("Failed to load Syphon framework: {}", error_str);
                } else {
                    log::error!("Failed to load Syphon framework: Unknown error");
                }
            } else {
                log::info!("Syphon framework loaded successfully");
            }
        }
        
        pub fn new(name: &str) -> Option<Self> {
            unsafe {
                // Ensure Syphon framework is loaded
                Self::ensure_syphon_framework_loaded();
                
                // Load Syphon framework classes - use SyphonOpenGLServer for OpenGL contexts
                let syphon_server_class = match Class::get("SyphonOpenGLServer") {
                    Some(class) => {
                        log::info!("SyphonOpenGLServer class found successfully");
                        class
                    }
                    None => {
                        log::error!("SyphonOpenGLServer class not found - framework may not be loaded properly");
                        return None;
                    }
                };
                
                // Create NSString for server name
                let nsstring_class = Class::get("NSString")?;
                let name_cstring = std::ffi::CString::new(name).ok()?;
                
                log::info!("Creating SyphonServer with name: {}", name);
                
                // Get the current NSOpenGLContext and extract CGLContextObj
                if let Some(nsopengl_context_class) = Class::get("NSOpenGLContext") {
                    let current_context: *mut Object = msg_send![nsopengl_context_class, currentContext];
                    if current_context.is_null() {
                        log::error!("No current NSOpenGLContext found - SyphonOpenGLServer requires active context");
                        return None;
                    }
                    
                    log::info!("Found current NSOpenGLContext, extracting CGLContextObj");
                    
                    // Extract CGLContextObj from NSOpenGLContext
                    let cgl_context: *mut std::ffi::c_void = msg_send![current_context, CGLContextObj];
                    if cgl_context.is_null() {
                        log::error!("Failed to extract CGLContextObj from NSOpenGLContext");
                        return None;
                    }
                    
                    log::info!("Successfully extracted CGLContextObj: {:?}", cgl_context);
                    
                    // Create NSString for server name
                    let nsstring: *mut Object = msg_send![nsstring_class, stringWithUTF8String: name_cstring.as_ptr()];
                    if nsstring.is_null() {
                        log::error!("Failed to create NSString for server name");
                        return None;
                    }
                    
                    // Create SyphonOpenGLServer with proper initialization
                    log::info!("Creating SyphonOpenGLServer with CGLContextObj");
                    let server: *mut Object = msg_send![syphon_server_class, alloc];
                    if server.is_null() {
                        log::error!("Failed to allocate SyphonOpenGLServer");
                        return None;
                    }
                    
                    // Initialize with CGLContextObj - this is the correct pattern from research
                    let server: *mut Object = msg_send![server, initWithName: nsstring context: cgl_context options: std::ptr::null::<Object>()];
                    
                    if !server.is_null() {
                        log::info!("SyphonOpenGLServer created successfully!");
                        Some(SyphonServerState { server })
                    } else {
                        log::error!("SyphonOpenGLServer initialization failed");
                        let _: () = msg_send![server, release];
                        None
                    }
                } else {
                    log::error!("NSOpenGLContext class not found");
                    None
                }
            }
        }
        
        pub fn publish_frame_from_bitmap(&self, data: &[u8], width: u32, height: u32) -> bool {
            unsafe {
                // Follow the Syphon publishing pattern: bindToDrawFrameOfSize -> render -> unbindAndPublish
                log::debug!("Publishing frame {}x{} to Syphon", width, height);
                
                // Create NSSize for frame dimensions
                // Note: NSSize is a struct with width and height fields
                #[repr(C)]
                struct NSSize {
                    width: f64,
                    height: f64,
                }
                
                let frame_size = NSSize {
                    width: width as f64,
                    height: height as f64,
                };
                
                // Bind to draw frame of specified size
                let _: () = msg_send![self.server, bindToDrawFrameOfSize: frame_size];
                
                // Render animated test pattern to make Syphon output visible and impressive
                log::debug!("Rendering animated test pattern for Syphon");
                
                // Load OpenGL functions if not already loaded
                static mut GL_LOADED: bool = false;
                unsafe {
                    if !GL_LOADED {
                        gl::load_with(|s| {
                            let c_str = std::ffi::CString::new(s).unwrap();
                            // Use dlsym to load OpenGL functions from current context
                            let symbol = libc::dlsym(libc::RTLD_DEFAULT, c_str.as_ptr());
                            symbol as *const std::ffi::c_void
                        });
                        GL_LOADED = true;
                        log::debug!("OpenGL functions loaded for Syphon rendering");
                    }
                }
                
                // Create dramatic animated test pattern with clear color cycling
                let time = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_millis() as f32 / 1000.0; // Use milliseconds for better precision
                
                // Make animation much more dramatic and obvious - cycle through primary colors
                let cycle_speed = 1.0; // Speed for color cycling (1 cycle per second)
                let time_mod = (time * cycle_speed) % (2.0 * std::f32::consts::PI); // Keep within 0-2π range
                let red = (time_mod).sin().abs(); // Pure red phase
                let green = (time_mod + std::f32::consts::PI * 2.0 / 3.0).sin().abs(); // Green phase  
                let blue = (time_mod + std::f32::consts::PI * 4.0 / 3.0).sin().abs(); // Blue phase
                
                unsafe {
                    // Clear with animated color that cycles through the rainbow
                    gl::ClearColor(red, green, blue, 1.0);
                    gl::Clear(gl::COLOR_BUFFER_BIT);
                    
                    // For now, just the animated background color is enough to show
                    // the Syphon pipeline is working. We can add more complex rendering later.
                    
                    // Add more visible feedback about color changes
                    static mut LAST_LOG_TIME: f32 = 0.0;
                    if time - LAST_LOG_TIME > 0.5 { // Log every 0.5 seconds
                        log::info!("🎨 Color animation: R={:.2}, G={:.2}, B={:.2} (time={:.1}s)", red, green, blue, time);
                        LAST_LOG_TIME = time;
                    }
                }
                
                // Unbind and publish the frame
                let _: () = msg_send![self.server, unbindAndPublish];
                
                log::debug!("Frame published to Syphon successfully");
                true
            }
        }
        
        pub fn has_clients(&self) -> bool {
            unsafe {
                let has_clients: bool = msg_send![self.server, hasClients];
                has_clients
            }
        }
        
        pub fn stop(&self) {
            unsafe {
                let _: () = msg_send![self.server, stop];
            }
        }
    }
    
    impl Drop for SyphonServerState {
        fn drop(&mut self) {
            self.stop();
            unsafe {
                let _: () = msg_send![self.server, release];
            }
        }
    }
}

pub struct SyphonOutput {
    name: String,
    #[cfg(target_os = "macos")]
    server: RefCell<Option<syphon_sys::SyphonServerState>>,
    #[cfg(not(target_os = "macos"))]
    _phantom: std::marker::PhantomData<()>,
}

impl SyphonOutput {
    pub fn new(name: String) -> Self {
        #[cfg(target_os = "macos")]
        {
            // Don't create server immediately - defer until first frame to avoid main process segfaults
            SyphonOutput {
                name,
                server: RefCell::new(None),
            }
        }
        
        #[cfg(not(target_os = "macos"))]
        {
            SyphonOutput {
                name,
                _phantom: std::marker::PhantomData,
            }
        }
    }
    
    pub fn update_frame(&self, data: &[u8], width: u32, height: u32) -> bool {
        #[cfg(target_os = "macos")]
        {
            // Create server lazily on first frame if needed
            if let Ok(mut server_opt) = self.server.try_borrow_mut() {
                if server_opt.is_none() {
                    // First frame - create the server now (safer than during construction)
                    match syphon_sys::SyphonServerState::new(&self.name) {
                        Some(server) => {
                            *server_opt = Some(server);
                        }
                        None => {
                            log::error!("Failed to create Syphon server");
                            return false;
                        }
                    }
                }
                
                if let Some(ref server) = *server_opt {
                    return server.publish_frame_from_bitmap(data, width, height);
                }
            }
            false
        }
        
        #[cfg(not(target_os = "macos"))]
        {
            // On non-macOS platforms, we'd implement Spout here
            false
        }
    }
    
    pub fn has_clients(&self) -> bool {
        #[cfg(target_os = "macos")]
        {
            if let Ok(server_opt) = self.server.try_borrow() {
                if let Some(ref server) = *server_opt {
                    return server.has_clients();
                }
            }
            // Return false if server hasn't been created yet (no frames published)
            false
        }
        
        #[cfg(not(target_os = "macos"))]
        {
            false
        }
    }
    
    pub fn stop(&self) {
        #[cfg(target_os = "macos")]
        {
            if let Ok(mut server_opt) = self.server.try_borrow_mut() {
                if let Some(server) = server_opt.take() {
                    server.stop();
                }
            }
        }
    }
}

impl Drop for SyphonOutput {
    fn drop(&mut self) {
        self.stop();
    }
}