use anyhow::Result;
use log::{debug, error, info, warn};
use std::sync::{Arc, Mutex};

// CEF integration for web content rendering
#[cfg(feature = "cef")]
use cef::{
    app::{App, AppCallbacks},
    browser::{Browser, BrowserHost, BrowserSettings},
    client::{Client, ClientCallbacks},
    frame::Frame,
    render_handler::{RenderHandler, RenderHandlerCallbacks},
    settings::Settings,
    string::CefString,
    types::{PaintElementType, RectType},
    values::CommandLine,
    CefApp, CefMainArgs,
};

/// CEF browser state for off-screen rendering
pub struct CefBrowser {
    #[cfg(feature = "cef")]
    browser: Option<Browser>,
    width: u32,
    height: u32,
    frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>,
}

impl CefBrowser {
    pub fn new(width: u32, height: u32) -> Self {
        Self {
            #[cfg(feature = "cef")]
            browser: None,
            width,
            height,
            frame_callback: Arc::new(Mutex::new(None)),
        }
    }

    /// Set callback for when new frames are rendered
    pub fn set_frame_callback<F>(&self, callback: F)
    where
        F: Fn(&[u8], u32, u32) + Send + Sync + 'static,
    {
        if let Ok(mut cb) = self.frame_callback.lock() {
            *cb = Some(Box::new(callback));
        }
    }

    /// Initialize CEF with off-screen rendering
    pub fn initialize(&mut self) -> Result<()> {
        #[cfg(feature = "cef")]
        {
            info!("Initializing CEF for off-screen web content rendering");

            // CEF settings for off-screen rendering
            let mut settings = Settings::new();
            settings.set_log_severity(cef::types::LogSeverity::Info);
            settings.set_windowless_rendering_enabled(true); // Enable off-screen rendering
            settings.set_no_sandbox(true); // Disable sandbox for simplicity

            // Create CEF app
            let app = TestApp::new(self.frame_callback.clone());
            let cef_app = CefApp::new(app);

            // Initialize CEF with main arguments
            let args = std::env::args().collect::<Vec<_>>();
            let main_args = CefMainArgs::new(args);

            // Initialize CEF
            match cef::initialize(&main_args, &settings, &cef_app, None) {
                Ok(_) => {
                    info!("CEF initialized successfully for off-screen rendering");
                }
                Err(e) => {
                    error!("Failed to initialize CEF: {:?}", e);
                    return Err(anyhow::anyhow!("CEF initialization failed: {:?}", e));
                }
            }

            // Create browser client with render handler
            let client = TestClient::new(self.frame_callback.clone());

            // Browser settings for off-screen rendering
            let mut browser_settings = BrowserSettings::new();
            browser_settings.set_windowless_frame_rate(60); // 60 FPS for smooth animation

            // Create off-screen browser
            let window_info = cef::window_info::WindowInfo::new_windowless();
            let url = CefString::new("https://www.google.com"); // Default URL

            match Browser::create_browser_sync(
                &window_info,
                &client,
                &url,
                &browser_settings,
                None,
                None,
            ) {
                Ok(browser) => {
                    info!("CEF browser created successfully for off-screen rendering");
                    self.browser = Some(browser);
                }
                Err(e) => {
                    error!("Failed to create CEF browser: {:?}", e);
                    return Err(anyhow::anyhow!("CEF browser creation failed: {:?}", e));
                }
            }

            Ok(())
        }

        #[cfg(not(feature = "cef"))]
        {
            warn!("CEF feature not enabled, using test patterns");
            Ok(())
        }
    }

    /// Load a URL in the browser
    pub fn load_url(&self, url: &str) -> Result<()> {
        #[cfg(feature = "cef")]
        {
            if let Some(ref browser) = self.browser {
                let frame = browser.get_main_frame();
                if let Some(frame) = frame {
                    let cef_url = CefString::new(url);
                    frame.load_url(&cef_url);
                    info!("Loading URL in CEF browser: {}", url);
                } else {
                    warn!("No main frame available for URL loading");
                }
            } else {
                warn!("CEF browser not initialized");
            }
        }

        #[cfg(not(feature = "cef"))]
        {
            info!("CEF not available, would load URL: {}", url);
        }

        Ok(())
    }

    /// Resize the browser viewport
    pub fn resize(&mut self, width: u32, height: u32) {
        self.width = width;
        self.height = height;

        #[cfg(feature = "cef")]
        {
            if let Some(ref browser) = self.browser {
                let host = browser.get_host();
                if let Some(host) = host {
                    host.was_resized();
                    debug!("CEF browser resized to {}x{}", width, height);
                }
            }
        }
    }

    /// Shutdown CEF
    pub fn shutdown(&self) {
        #[cfg(feature = "cef")]
        {
            info!("Shutting down CEF browser");
            cef::shutdown();
        }
    }
}

impl Drop for CefBrowser {
    fn drop(&mut self) {
        self.shutdown();
    }
}

#[cfg(feature = "cef")]
struct TestApp {
    frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>,
}

#[cfg(feature = "cef")]
impl TestApp {
    fn new(frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>) -> Self {
        Self { frame_callback }
    }
}

#[cfg(feature = "cef")]
impl AppCallbacks for TestApp {
    fn on_before_command_line_processing(
        &self,
        _process_type: &CefString,
        _command_line: &CommandLine,
    ) {
        // Configure command line arguments if needed
    }
}

#[cfg(feature = "cef")]
struct TestClient {
    render_handler: TestRenderHandler,
}

#[cfg(feature = "cef")]
impl TestClient {
    fn new(frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>) -> Self {
        Self {
            render_handler: TestRenderHandler::new(frame_callback),
        }
    }
}

#[cfg(feature = "cef")]
impl ClientCallbacks for TestClient {
    fn get_render_handler(&self) -> Option<&dyn RenderHandler> {
        Some(&self.render_handler)
    }
}

#[cfg(feature = "cef")]
struct TestRenderHandler {
    frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>,
}

#[cfg(feature = "cef")]
impl TestRenderHandler {
    fn new(frame_callback: Arc<Mutex<Option<Box<dyn Fn(&[u8], u32, u32) + Send + Sync>>>>) -> Self {
        Self { frame_callback }
    }
}

#[cfg(feature = "cef")]
impl RenderHandlerCallbacks for TestRenderHandler {
    fn get_view_rect(&self, _browser: &Browser) -> RectType {
        // Return the viewport size for off-screen rendering
        RectType {
            x: 0,
            y: 0,
            width: 1280,
            height: 720,
        }
    }

    fn on_paint(
        &self,
        _browser: &Browser,
        paint_element_type: PaintElementType,
        _dirty_rects: &[RectType],
        buffer: &[u8],
        width: i32,
        height: i32,
    ) {
        // This is the key callback - captures rendered web content
        if paint_element_type == PaintElementType::View {
            debug!(
                "CEF rendered frame: {}x{} ({} bytes)",
                width,
                height,
                buffer.len()
            );

            // Call the frame callback to send data to Syphon
            if let Ok(callback_opt) = self.frame_callback.lock() {
                if let Some(ref callback) = *callback_opt {
                    callback(buffer, width as u32, height as u32);
                }
            }
        }
    }
}