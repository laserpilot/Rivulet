use std::cell::RefCell;

// Forward declare the native Syphon types we'll use
#[cfg(target_os = "macos")]
mod syphon_sys {
    use std::ffi::c_void;
    
    #[repr(C)]
    pub struct SyphonServerState(*const c_void);
    
    unsafe impl Send for SyphonServerState {}
    unsafe impl Sync for SyphonServerState {}
    
    // FFI declarations to our Objective-C bridge
    extern "C" {
        pub fn syphon_server_create(name: *const i8) -> *mut SyphonServerState;
        pub fn syphon_server_publish_frame(server: *mut SyphonServerState, data: *const u8, width: u32, height: u32) -> bool;
        pub fn syphon_server_has_clients(server: *mut SyphonServerState) -> bool;
        pub fn syphon_server_stop(server: *mut SyphonServerState);
    }
}

pub struct SyphonOutput {
    name: String,
    #[cfg(target_os = "macos")]
    server: RefCell<Option<*mut syphon_sys::SyphonServerState>>,
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
                    let c_name = std::ffi::CString::new(self.name.clone()).unwrap();
                    let server = unsafe { syphon_sys::syphon_server_create(c_name.as_ptr()) };
                    
                    if !server.is_null() {
                        *server_opt = Some(server);
                    } else {
                        return false;
                    }
                }
                
                if let Some(server) = *server_opt {
                    unsafe {
                        return syphon_sys::syphon_server_publish_frame(server, data.as_ptr(), width, height);
                    }
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
                if let Some(server) = *server_opt {
                    unsafe {
                        return syphon_sys::syphon_server_has_clients(server);
                    }
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
                    unsafe {
                        syphon_sys::syphon_server_stop(server);
                    }
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

// Note: Neon JavaScript interface implementations removed for binary target usage
// The JS interface is still available in lib.rs for Node.js module usage