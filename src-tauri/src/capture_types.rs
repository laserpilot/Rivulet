use std::os::raw::c_char;

#[repr(C)]
pub struct CapturedWindow {
    pub window_id: u32,
    pub app_pid: i32,
    pub app_name: *const c_char,
    pub window_title: *const c_char,
}

#[repr(C)]
pub struct WindowList {
    pub windows: *const CapturedWindow,
    pub count: usize,
}
