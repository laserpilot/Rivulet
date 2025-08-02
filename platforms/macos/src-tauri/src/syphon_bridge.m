#define GL_SILENCE_DEPRECATION
#import <Foundation/Foundation.h>
#import <AppKit/NSOpenGL.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/CGLContext.h>    // For CGLGetError()
#import <IOSurface/IOSurface.h>
#import <OpenGL/CGLIOSurface.h>  // For CGLTexImageIOSurface2D
#import <CoreVideo/CoreVideo.h>  // For kCVPixelFormatType_32BGRA
#import <ScreenCaptureKit/ScreenCaptureKit.h>  // For SCShareableContent, SCWindow
#include <unistd.h>  // For getpid()

// Manually define GL_TEXTURE_RECTANGLE as it's often missing in modern headers
#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif

// Use the modern, specific Syphon class for OpenGL
#import <Syphon/SyphonOpenGLServer.h>

#include <stdbool.h>

// Define a C-compatible struct for the Syphon server state
typedef struct {
    SyphonOpenGLServer* server;
} SyphonServerState;

// C interface functions that Rust can call
SyphonServerState* syphon_server_create(const char* name, const void* ctx) {
    @autoreleasepool {
        CGLContextObj cgl_ctx = (CGLContextObj)ctx;
        if (cgl_ctx == NULL) {
            NSLog(@"❌ ERROR: syphon_server_create received a NULL context.");
            return NULL;
        }

        SyphonServerState* state = malloc(sizeof(SyphonServerState));
        if (!state) return NULL;

        NSString* serverName = [NSString stringWithUTF8String:name];
        NSLog(@"🎬 Creating SyphonOpenGLServer with provided context: %@", serverName);

        @try {
            state->server = [[SyphonOpenGLServer alloc] initWithName:serverName
                                                             context:cgl_ctx
                                                             options:nil];
            if (state->server) {
                NSLog(@"✅ SyphonOpenGLServer created successfully: %@", serverName);
            } else {
                NSLog(@"❌ Failed to create SyphonOpenGLServer.");
                free(state);
                return NULL;
            }
        } @catch (NSException *exception) {
            NSLog(@"❌ Exception creating SyphonOpenGLServer: %@", exception);
            free(state);
            return NULL;
        }

        return state;
    }
}

// PHASE G: Get window ID by title prefix (for content window targeting)
uint32_t get_window_by_title_prefix(const char* titlePrefix) {
    @autoreleasepool {
        if (@available(macOS 12.3, *)) {
            __block uint32_t foundWindowID = 0;
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content, NSError *error) {
                if (error) {
                    NSLog(@"❌ Error getting shareable content: %@", error.localizedDescription);
                } else {
                    NSString *searchPrefix = [NSString stringWithUTF8String:titlePrefix];
                    
                    // Search through all windows
                    for (SCWindow *window in content.windows) {
                        NSString *windowTitle = window.title;
                        
                        if (windowTitle && [windowTitle hasPrefix:searchPrefix]) {
                            // Found a matching window
                            NSLog(@"✅ Found window with title prefix '%@': Window ID %u, Title: '%@'", 
                                  searchPrefix, window.windowID, windowTitle);
                            foundWindowID = window.windowID;
                            break; // Take the first match
                        }
                    }
                    
                    if (foundWindowID == 0) {
                        NSLog(@"⚠️ No window found with title prefix: %@", searchPrefix);
                    }
                }
                dispatch_semaphore_signal(semaphore);
            }];
            
            // Wait for completion (with timeout)
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
            if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
                NSLog(@"❌ Timeout waiting for window search by title prefix");
                return 0;
            }
            
            return foundWindowID;
        } else {
            NSLog(@"❌ ScreenCaptureKit requires macOS 12.3 or later");
            return 0;
        }
    }
}

// ZERO-COPY HELPER: Create IOSurface for testing
IOSurfaceRef create_test_iosurface(unsigned int width, unsigned int height) {
    @autoreleasepool {
        NSDictionary *properties = @{
            (__bridge NSString *)kIOSurfaceWidth: @(width),
            (__bridge NSString *)kIOSurfaceHeight: @(height),
            (__bridge NSString *)kIOSurfacePixelFormat: @(kCVPixelFormatType_32BGRA),
        };
        return IOSurfaceCreate((__bridge CFDictionaryRef)properties);
    }
}

bool syphon_server_publish_iosurface(SyphonServerState* state, const void* iosurface_ref) {
    if (!state || !state->server || !iosurface_ref) return false;

    @autoreleasepool {
        IOSurfaceRef surface = (IOSurfaceRef)iosurface_ref;
        size_t width = IOSurfaceGetWidth(surface);
        size_t height = IOSurfaceGetHeight(surface);

        @try {
            // DON'T switch contexts - Rust manages the dedicated context properly
            // The Syphon server already has its own dedicated context from Rust
            CGLContextObj syphon_context = [state->server context];
            
            // 1. Create a temporary GL texture to bind the IOSurface to
            GLuint temp_texture;
            glGenTextures(1, &temp_texture);
            glBindTexture(GL_TEXTURE_RECTANGLE, temp_texture);

            // 2. Use CGLTexImageIOSurface2D with the dedicated context (no switching needed)
            CGLError cgl_error = CGLTexImageIOSurface2D(
                syphon_context,
                GL_TEXTURE_RECTANGLE,
                GL_RGBA8,  // Internal format
                (GLsizei)width,
                (GLsizei)height,
                GL_BGRA,   // External format (IOSurface is BGRA)
                GL_UNSIGNED_INT_8_8_8_8_REV,  // Type
                surface,
                0  // plane
            );

            if (cgl_error != kCGLNoError) {
                NSLog(@"❌ CGLTexImageIOSurface2D failed with error: %d. This should not happen with a dedicated context.", cgl_error);
                glDeleteTextures(1, &temp_texture);
                return false;
            }

            // 3. Publish the texture that now represents the IOSurface
            [state->server publishFrameTexture:temp_texture
                                 textureTarget:GL_TEXTURE_RECTANGLE
                                   imageRegion:NSMakeRect(0, 0, width, height)
                              textureDimensions:NSMakeSize(width, height)
                                       flipped:YES];

            // 4. Clean up the temporary texture
            glDeleteTextures(1, &temp_texture);
            glBindTexture(GL_TEXTURE_RECTANGLE, 0);

            return true;
        } @catch (NSException *exception) {
            NSLog(@"❌ Exception during IOSurface publishing: %@", exception);
            return false;
        }
    }
}

// Legacy CPU-based publishing path
bool syphon_server_publish_frame(SyphonServerState* state, const unsigned char* data, 
                                 unsigned int width, unsigned int height) {
    if (!state || !state->server || !data) return false;

    @autoreleasepool {
        // This function is not used in the zero-copy path, but is kept for compatibility
        // and to satisfy the linker. It is not recommended for high performance.
        return false; // Return false to indicate it's not the primary path
    }
}

bool syphon_server_has_clients(SyphonServerState* state) {
    if (!state || !state->server) return false;
    @autoreleasepool {
        return [state->server hasClients];
    }
}

void syphon_server_stop(SyphonServerState* state) {
    if (!state) return;
    @autoreleasepool {
        if (state->server) {
            [state->server stop];
            state->server = nil;
        }
        free(state);
    }
}

// Forward declarations for screen capture functions
extern bool screencapture_initialize(void);
extern bool screencapture_initialize_window(uint32_t windowID);  // DEPRECATED
extern bool screencapture_initialize_application(void);          // Application-based capture
extern bool screencapture_initialize_application_window(uint32_t windowID);  // PHASE E.2: Window-specific capture
extern bool screencapture_initialize_content_only_window(uint32_t windowID);  // PHASE E.3: Content-only capture
extern IOSurfaceRef screencapture_get_latest_frame(void);
extern bool screencapture_has_frame(void);
extern void screencapture_stop(void);

// REAL SCREEN CAPTURE: Publish actual screen content via zero-copy IOSurface
bool syphon_server_publish_screen_capture(SyphonServerState* state) {
    if (!state) return false;
    
    @autoreleasepool {
        // Get latest screen capture frame
        IOSurfaceRef surface = screencapture_get_latest_frame();
        if (!surface) {
            return false; // No frame available
        }
        
        // Use the zero-copy IOSurface publishing path
        bool success = syphon_server_publish_iosurface(state, surface);
        
        // Release our reference (the IOSurface may still be used by Syphon)
        CFRelease(surface);
        
        return success;
    }
}

// Initialize screen capture system
bool syphon_server_start_screen_capture(void) {
    return screencapture_initialize();
}

// Initialize window capture system (DEPRECATED)
bool syphon_server_start_window_capture(uint32_t windowID) {
    return screencapture_initialize_window(windowID);
}

// Initialize application-based capture system (NEW - PREFERRED METHOD)
bool syphon_server_start_application_capture(void) {
    return screencapture_initialize_application();
}

// PHASE E.2: Initialize window-specific capture system
bool syphon_server_start_application_window_capture(uint32_t windowID) {
    return screencapture_initialize_application_window(windowID);
}

// PHASE E.3: Initialize content-only capture system (excludes decorations)
bool syphon_server_start_content_only_window_capture(uint32_t windowID) {
    return screencapture_initialize_content_only_window(windowID);
}

// Stop screen capture system
void syphon_server_stop_screen_capture(void) {
    screencapture_stop();
}

// Check if screen capture has frames available
bool syphon_server_has_screen_frame(void) {
    return screencapture_has_frame();
}

// PHASE D: Simple data bridge - get frontmost window ID
uint32_t get_frontmost_window_id(void) {
    @autoreleasepool {
        if (@available(macOS 12.3, *)) {
            __block uint32_t foundWindowID = 0;
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            
            // Get shareable content to find windows
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Error getting shareable content: %@", error.localizedDescription);
                    dispatch_semaphore_signal(semaphore);
                    return;
                }
                
                // Get current process ID to avoid selecting our own window
                pid_t currentPID = getpid();
                
                // Find the first valid window that isn't our own app or the desktop
                for (SCWindow *window in content.windows) {
                    // Skip windows from our own application
                    if (window.owningApplication && window.owningApplication.processID == currentPID) {
                        continue;
                    }
                    
                    // Skip desktop and system windows
                    if (!window.owningApplication || 
                        [window.owningApplication.applicationName isEqualToString:@"WindowServer"] ||
                        [window.owningApplication.applicationName isEqualToString:@"Dock"]) {
                        continue;
                    }
                    
                    // Found a valid window
                    foundWindowID = window.windowID;
                    NSLog(@"🔍 Found frontmost window: ID=%u, App=%@, Title=%@", 
                          window.windowID, 
                          window.owningApplication.applicationName,
                          window.title ?: @"(no title)");
                    break;
                }
                
                dispatch_semaphore_signal(semaphore);
            }];
            
            // Wait for completion (with timeout)
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);
            if (dispatch_semaphore_wait(semaphore, timeout) == 0) {
                return foundWindowID;
            } else {
                NSLog(@"❌ Timeout waiting for shareable content");
                return 0;
            }
        } else {
            NSLog(@"❌ ScreenCaptureKit not available (requires macOS 12.3+)");
            return 0;
        }
    }
}

// PHASE E: Get our own application's main window information  
uint32_t get_our_window_info(void) {
    @autoreleasepool {
        if (@available(macOS 12.3, *)) {
            __block uint32_t foundWindowID = 0;
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            
            // Get shareable content to find our own windows
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Error getting shareable content: %@", error.localizedDescription);
                    dispatch_semaphore_signal(semaphore);
                    return;
                }
                
                // Get current process ID to find our own window
                pid_t currentPID = getpid();
                
                // Find our application's main window
                for (SCWindow *window in content.windows) {
                    // Only look at windows from our own application
                    if (window.owningApplication && window.owningApplication.processID == currentPID) {
                        // Skip windows that are too small (likely utility windows)
                        if (window.frame.size.width < 100 || window.frame.size.height < 100) {
                            continue;
                        }
                        
                        // Found our main window (first valid window from our app)
                        foundWindowID = window.windowID;
                        NSLog(@"🎯 Found our window: ID=%u, App=%@, Title=%@, Size=%.0fx%.0f", 
                              window.windowID, 
                              window.owningApplication.applicationName,
                              window.title ?: @"(no title)",
                              window.frame.size.width,
                              window.frame.size.height);
                        break;
                    }
                }
                
                dispatch_semaphore_signal(semaphore);
            }];
            
            // Wait for completion (with timeout)
            dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);
            if (dispatch_semaphore_wait(semaphore, timeout) == 0) {
                return foundWindowID;
            } else {
                NSLog(@"❌ Timeout waiting for shareable content");
                return 0;
            }
        } else {
            NSLog(@"❌ ScreenCaptureKit not available (requires macOS 12.3+)");
            return 0;
        }
    }
}