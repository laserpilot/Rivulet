// Real screen capture implementation for zero-copy IOSurface capture
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>
#import <IOSurface/IOSurface.h>
#import <os/lock.h>
#include <unistd.h>

// Global capture session state
static AVCaptureSession *captureSession = nil;        // Legacy AVFoundation capture
static AVCaptureScreenInput *screenInput = nil;       // Legacy screen input
static AVCaptureVideoDataOutput *videoOutput = nil;   // Legacy video output
static SCStream *scStream = nil;                       // Modern ScreenCaptureKit stream
static dispatch_queue_t captureQueue = nil;
static IOSurfaceRef latestFrame = NULL;
static os_unfair_lock frameLock = OS_UNFAIR_LOCK_INIT;

// Legacy AVFoundation delegate to receive frames
@interface ScreenCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@end

// Modern ScreenCaptureKit delegate to receive frames
@interface ScreenCaptureKitDelegate : NSObject <SCStreamDelegate, SCStreamOutput>
@end

@implementation ScreenCaptureDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    // Extract IOSurface from the sample buffer
    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (pixelBuffer) {
        IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);
        if (surface) {
            // Thread-safe update of latest frame
            os_unfair_lock_lock(&frameLock);
            if (latestFrame) {
                CFRelease(latestFrame);
            }
            latestFrame = surface;
            CFRetain(latestFrame);
            os_unfair_lock_unlock(&frameLock);
        }
    }
}

@end

@implementation ScreenCaptureKitDelegate

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    // Add debugging to see if we're receiving ANY frames at all
    static int frameCount = 0;
    frameCount++;
    if (frameCount <= 5) {
        NSLog(@"🔍 Phase E.3 DEBUG: Received frame %d, type: %d", frameCount, (int)type);
    }
    
    if (type == SCStreamOutputTypeScreen) {
        // Extract IOSurface from the sample buffer (same as legacy method)
        CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (pixelBuffer) {
            IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);
            if (surface) {
                // Log actual content dimensions on first frame to verify content-only capture
                static BOOL loggedDimensions = NO;
                if (!loggedDimensions) {
                    size_t width = CVPixelBufferGetWidth(pixelBuffer);
                    size_t height = CVPixelBufferGetHeight(pixelBuffer);
                    NSLog(@"🎉 SUCCESS: Receiving CONTENT-ONLY frames at %zux%zu", width, height);
                    NSLog(@"📏 Frame Analysis: This should be smaller than window frame (decorations excluded)");
                    NSLog(@"✨ No occlusion, no title bar, no manual cropping - pure content capture!");
                    loggedDimensions = YES;
                }
                
                // Thread-safe update of latest frame
                os_unfair_lock_lock(&frameLock);
                if (latestFrame) {
                    CFRelease(latestFrame);
                }
                latestFrame = surface;
                CFRetain(latestFrame);
                os_unfair_lock_unlock(&frameLock);
            } else {
                NSLog(@"⚠️ Phase E.3 DEBUG: pixelBuffer found but no IOSurface");
            }
        } else {
            // Reduce log spam - only log first few occurrences
            static int missingPixelBufferCount = 0;
            missingPixelBufferCount++;
            if (missingPixelBufferCount <= 3) {
                NSLog(@"⚠️ Phase E.3: No pixelBuffer in sampleBuffer (occurrence %d)", missingPixelBufferCount);
                if (missingPixelBufferCount == 3) {
                    NSLog(@"⚠️ Further missing pixelBuffer warnings will be suppressed");
                }
            }
        }
    } else {
        NSLog(@"⚠️ Phase E.3 DEBUG: Non-screen frame type: %d", (int)type);
    }
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
    if (error) {
        NSLog(@"❌ ScreenCaptureKit stream stopped with error: %@", error.localizedDescription);
    } else {
        NSLog(@"✅ ScreenCaptureKit stream stopped successfully");
    }
}

@end

static ScreenCaptureDelegate *captureDelegate = nil;
static ScreenCaptureKitDelegate *screenCaptureKitDelegate = nil;

// Forward declarations
void screencapture_stop(void);

// Initialize screen capture using AVFoundation (desktop capture)
bool screencapture_initialize(void) {
    @autoreleasepool {
        if (captureSession) {
            return true; // Already initialized
        }
        
        // Create capture queue
        captureQueue = dispatch_queue_create("screencapture.queue", DISPATCH_QUEUE_SERIAL);
        
        // Create capture session
        captureSession = [[AVCaptureSession alloc] init];
        captureSession.sessionPreset = AVCaptureSessionPresetHigh;
        
        // Create screen input (desktop capture)
        CGDirectDisplayID displayID = CGMainDisplayID();
        screenInput = [[AVCaptureScreenInput alloc] initWithDisplayID:displayID];
        
        if (!screenInput) {
            NSLog(@"❌ Failed to create screen input");
            return false;
        }
        
        // Configure screen input for high performance
        screenInput.minFrameDuration = CMTimeMake(1, 60); // 60 FPS
        screenInput.capturesCursor = YES;
        screenInput.capturesMouseClicks = NO;
        
        // Add screen input to session
        if (![captureSession canAddInput:screenInput]) {
            NSLog(@"❌ Cannot add screen input to capture session");
            return false;
        }
        [captureSession addInput:screenInput];
        
        // Create video output
        videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        
        // Configure video output for IOSurface delivery
        NSDictionary *videoSettings = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{}
        };
        videoOutput.videoSettings = videoSettings;
        
        // Create capture delegate
        captureDelegate = [[ScreenCaptureDelegate alloc] init];
        [videoOutput setSampleBufferDelegate:captureDelegate queue:captureQueue];
        
        // Add video output to session
        if (![captureSession canAddOutput:videoOutput]) {
            NSLog(@"❌ Cannot add video output to capture session");
            return false;
        }
        [captureSession addOutput:videoOutput];
        
        // Start capture session
        [captureSession startRunning];
        
        if (captureSession.isRunning) {
            NSLog(@"✅ AVFoundation screen capture started at 60fps");
            return true;
        } else {
            NSLog(@"❌ Failed to start capture session");
            return false;
        }
    }
}

// NEW PHASE E.2: Initialize application capture for specific window using ScreenCaptureKit  
bool screencapture_initialize_application_window(uint32_t windowID) {
    @autoreleasepool {
        // Stop any existing sessions first
        if (captureSession) {
            screencapture_stop();
        }
        if (scStream) {
            [scStream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Error stopping existing SCStream: %@", error.localizedDescription);
                }
            }];
            scStream = nil;
        }
        
        NSLog(@"🎯 Initializing window-specific capture using ScreenCaptureKit for window ID: %u", windowID);
        
        // Create capture queue
        captureQueue = dispatch_queue_create("screencapture.window.specific.queue", DISPATCH_QUEUE_SERIAL);
        
        // Use ScreenCaptureKit for window-specific capture (macOS 12.3+)
        if (@available(macOS 12.3, *)) {
            // Get current process ID to verify window ownership
            pid_t currentPID = getpid();
            NSLog(@"🔍 Current process PID: %d, target window ID: %u", currentPID, windowID);
            
            // Get shareable content to find our specific window
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Failed to get shareable content: %@", error.localizedDescription);
                    return;
                }
                
                // Find our specific window by ID
                SCWindow *targetWindow = nil;
                for (SCWindow *window in content.windows) {
                    if (window.windowID == windowID) {
                        // Verify this window belongs to our application
                        if (window.owningApplication && window.owningApplication.processID == currentPID) {
                            targetWindow = window;
                            NSLog(@"✅ Found target window: ID=%u, App=%@, Title=%@, Size=%.0fx%.0f", 
                                  window.windowID,
                                  window.owningApplication.applicationName,
                                  window.title ?: @"(no title)",
                                  window.frame.size.width,
                                  window.frame.size.height);
                            break;
                        } else {
                            NSLog(@"⚠️ Window ID %u found but belongs to different application (PID: %d)", 
                                  windowID, window.owningApplication ? window.owningApplication.processID : -1);
                        }
                    }
                }
                
                if (!targetWindow) {
                    NSLog(@"❌ Could not find window ID %u in our application", windowID);
                    return;
                }
                
                // Create content filter for our application (foundation for window-specific capture)
                // Use application-based filtering - Phase E.3 will add window exclusions for content-only capture
                NSArray<SCRunningApplication *> *applications = @[targetWindow.owningApplication];
                SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject 
                                                             includingApplications:applications 
                                                                    exceptingWindows:@[]];
                
                NSLog(@"🎯 Created content filter for application: %@ (window ID: %u)", 
                      targetWindow.owningApplication.applicationName, windowID);
                
                // Configure stream settings for high performance
                SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
                // Use the actual window size for optimal capture
                config.width = (NSInteger)targetWindow.frame.size.width;
                config.height = (NSInteger)targetWindow.frame.size.height;
                config.pixelFormat = kCVPixelFormatType_32BGRA;  // BGRA for optimal GPU performance
                config.showsCursor = YES;
                config.capturesAudio = NO;
                config.sampleRate = 60;  // 60 FPS
                config.minimumFrameInterval = CMTimeMake(1, 60);
                
                NSLog(@"🎯 Configuring capture for window: %dx%d", (int)config.width, (int)config.height);
                
                // Create the stream
                scStream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:screenCaptureKitDelegate];
                
                if (!screenCaptureKitDelegate) {
                    screenCaptureKitDelegate = [[ScreenCaptureKitDelegate alloc] init];
                }
                
                // Add output for receiving frames
                NSError *outputError = nil;
                [scStream addStreamOutput:screenCaptureKitDelegate type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue error:&outputError];
                if (outputError) {
                    NSLog(@"❌ Failed to add stream output: %@", outputError.localizedDescription);
                    return;
                }
                
                // Start capture
                [scStream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                    if (error) {
                        NSLog(@"❌ Failed to start ScreenCaptureKit window capture: %@", error.localizedDescription);
                    } else {
                        NSLog(@"✅ ScreenCaptureKit window-specific capture started for window %u!", windowID);
                    }
                }];
            }];
            
            return true;  // Return immediately, actual success/failure handled in completion handlers
        } else {
            NSLog(@"❌ ScreenCaptureKit not available on this macOS version (requires 12.3+)");
            return false;
        }
    }
}

// Initialize application-based capture using ScreenCaptureKit
bool screencapture_initialize_application(void) {
    @autoreleasepool {
        // Stop any existing sessions first
        if (captureSession) {
            screencapture_stop();
        }
        if (scStream) {
            [scStream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Error stopping existing SCStream: %@", error.localizedDescription);
                }
            }];
            scStream = nil;
        }
        
        NSLog(@"🎯 Initializing application-based capture using ScreenCaptureKit");
        
        // Create capture queue
        captureQueue = dispatch_queue_create("screencapture.application.queue", DISPATCH_QUEUE_SERIAL);
        
        // Use ScreenCaptureKit for application-based capture (macOS 12.3+)
        if (@available(macOS 12.3, *)) {
            // Get current process ID
            pid_t currentPID = getpid();
            NSLog(@"🔍 Current process PID: %d", currentPID);
            
            // Get shareable content to find our application
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Failed to get shareable content: %@", error.localizedDescription);
                    return;
                }
                
                // Find our application by PID
                SCRunningApplication *ourApp = nil;
                for (SCRunningApplication *app in content.applications) {
                    if (app.processID == currentPID) {
                        ourApp = app;
                        NSLog(@"✅ Found our application: %@ (PID: %d)", app.applicationName, app.processID);
                        break;
                    }
                }
                
                if (!ourApp) {
                    NSLog(@"❌ Could not find our application in shareable content");
                    return;
                }
                
                // Create content filter for our application
                NSArray<SCRunningApplication *> *applications = @[ourApp];
                SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject 
                                                              includingApplications:applications 
                                                                    exceptingWindows:@[]];
                
                // Configure stream settings for high performance
                SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
                config.width = 1920;  // Reasonable default, will be adjusted by system
                config.height = 1080;
                config.pixelFormat = kCVPixelFormatType_32BGRA;  // BGRA for optimal GPU performance
                config.showsCursor = YES;
                config.capturesAudio = NO;
                config.sampleRate = 60;  // 60 FPS
                config.minimumFrameInterval = CMTimeMake(1, 60);
                
                // Create the stream
                scStream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:screenCaptureKitDelegate];
                
                if (!screenCaptureKitDelegate) {
                    screenCaptureKitDelegate = [[ScreenCaptureKitDelegate alloc] init];
                }
                
                // Add output for receiving frames
                NSError *outputError = nil;
                [scStream addStreamOutput:screenCaptureKitDelegate type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue error:&outputError];
                if (outputError) {
                    NSLog(@"❌ Failed to add stream output: %@", outputError.localizedDescription);
                    return;
                }
                
                // Start capture
                [scStream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                    if (error) {
                        NSLog(@"❌ Failed to start ScreenCaptureKit capture: %@", error.localizedDescription);
                    } else {
                        NSLog(@"✅ ScreenCaptureKit application capture started successfully!");
                    }
                }];
            }];
            
            return true;  // Return immediately, actual success/failure handled in completion handlers
        } else {
            NSLog(@"❌ ScreenCaptureKit not available on this macOS version (requires 12.3+)");
            return false;
        }
    }
}

// Legacy window-specific capture (DEPRECATED - kept for compatibility)
bool screencapture_initialize_window(uint32_t windowID) {
    @autoreleasepool {
        NSLog(@"⚠️ screencapture_initialize_window is deprecated, use screencapture_initialize_application instead");
        
        if (captureSession) {
            // Stop existing session first
            screencapture_stop();
        }
        
        NSLog(@"🎯 Initializing window capture for CGWindowID: %u", windowID);
        
        // Create capture queue
        captureQueue = dispatch_queue_create("screencapture.window.queue", DISPATCH_QUEUE_SERIAL);
        
        // Create capture session
        captureSession = [[AVCaptureSession alloc] init];
        captureSession.sessionPreset = AVCaptureSessionPresetHigh;
        
        // Fallback to display capture (legacy behavior)
        CGDirectDisplayID displayID = CGMainDisplayID();
        screenInput = [[AVCaptureScreenInput alloc] initWithDisplayID:displayID];
        
        if (!screenInput) {
            NSLog(@"❌ Failed to create screen input for window capture");
            return false;
        }
        
        // Configure screen input for high performance
        screenInput.minFrameDuration = CMTimeMake(1, 60); // 60 FPS
        screenInput.capturesCursor = YES;
        screenInput.capturesMouseClicks = NO;
        
        // Add screen input to session
        if (![captureSession canAddInput:screenInput]) {
            NSLog(@"❌ Cannot add screen input to capture session for window");
            return false;
        }
        [captureSession addInput:screenInput];
        
        // Create video output
        videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        
        // Configure video output for IOSurface delivery
        NSDictionary *videoSettings = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{}
        };
        videoOutput.videoSettings = videoSettings;
        
        // Create capture delegate
        captureDelegate = [[ScreenCaptureDelegate alloc] init];
        [videoOutput setSampleBufferDelegate:captureDelegate queue:captureQueue];
        
        // Add video output to session
        if (![captureSession canAddOutput:videoOutput]) {
            NSLog(@"❌ Cannot add video output to capture session for window");
            return false;
        }
        [captureSession addOutput:videoOutput];
        
        // Start capture session
        [captureSession startRunning];
        
        if (captureSession.isRunning) {
            NSLog(@"✅ AVFoundation window capture started at 60fps for window %u", windowID);
            return true;
        } else {
            NSLog(@"❌ Failed to start capture session for window %u", windowID);
            return false;
        }
    }
}

// Get the latest captured frame as IOSurface
IOSurfaceRef screencapture_get_latest_frame(void) {
    IOSurfaceRef frame = NULL;
    
    os_unfair_lock_lock(&frameLock);
    if (latestFrame) {
        frame = latestFrame;
        CFRetain(frame); // Caller owns this reference
    }
    os_unfair_lock_unlock(&frameLock);
    
    return frame;
}

// Stop capture and cleanup
void screencapture_stop(void) {
    @autoreleasepool {
        if (captureSession) {
            [captureSession stopRunning];
            captureSession = nil;
            NSLog(@"📴 AVFoundation screen capture stopped");
        }
        
        os_unfair_lock_lock(&frameLock);
        if (latestFrame) {
            CFRelease(latestFrame);
            latestFrame = NULL;
        }
        os_unfair_lock_unlock(&frameLock);
        
        screenInput = nil;
        videoOutput = nil;
        captureDelegate = nil;
        
        if (captureQueue) {
            captureQueue = nil;
        }
    }
}

// PHASE E.3: Initialize content-only capture for specific window (excludes decorations)
bool screencapture_initialize_content_only_window(uint32_t windowID) {
    @autoreleasepool {
        // Stop any existing sessions first
        if (captureSession) {
            screencapture_stop();
        }
        if (scStream) {
            [scStream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Error stopping existing SCStream: %@", error.localizedDescription);
                }
            }];
            scStream = nil;
        }
        
        NSLog(@"🎯 Phase E.3: Initializing content-only capture (no decorations) for window ID: %u", windowID);
        
        // Create capture queue
        captureQueue = dispatch_queue_create("screencapture.content.only.queue", DISPATCH_QUEUE_SERIAL);
        
        // Use ScreenCaptureKit for content-only capture (macOS 12.3+)
        if (@available(macOS 12.3, *)) {
            // Get current process ID to verify window ownership
            pid_t currentPID = getpid();
            NSLog(@"🔍 Current process PID: %d, target window ID: %u", currentPID, windowID);
            
            // Get shareable content to find our specific window
            [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent * _Nullable content, NSError * _Nullable error) {
                if (error) {
                    NSLog(@"❌ Failed to get shareable content: %@", error.localizedDescription);
                    return;
                }
                
                // Find our specific window by ID - simplified since framework handles decorations
                SCWindow *targetWindow = nil;
                
                for (SCWindow *window in content.windows) {
                    if (window.windowID == windowID) {
                        // Verify this window belongs to our application
                        if (window.owningApplication && window.owningApplication.processID == currentPID) {
                            targetWindow = window;
                            NSLog(@"✅ Found target window: ID=%u, App=%@, Title=%@, Frame=%.0fx%.0f", 
                                  window.windowID,
                                  window.owningApplication.applicationName,
                                  window.title ?: @"(no title)",
                                  window.frame.size.width,
                                  window.frame.size.height);
                            break;
                        } else {
                            NSLog(@"⚠️ Window ID %u found but belongs to different application (PID: %d)", 
                                  windowID, window.owningApplication ? window.owningApplication.processID : -1);
                        }
                    }
                }
                
                if (!targetWindow) {
                    NSLog(@"❌ Could not find window ID %u in our application", windowID);
                    return;
                }
                
                NSLog(@"🎯 Phase E.3: Using Apple's automatic decoration exclusion - no manual filtering needed");
                
                // PHASE E.3 PROPER SOLUTION: Use initWithDesktopIndependentWindow for content-only capture
                // This method automatically excludes window decorations (title bar, borders, shadows)
                SCContentFilter *filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:targetWindow];
                
                NSLog(@"🎯 Phase E.3: Created desktop-independent filter for content-only capture of window: %@ (ID: %u)", 
                      targetWindow.owningApplication.applicationName, windowID);
                
                // Configure stream settings - Let Apple determine the optimal content dimensions
                SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
                
                // DO NOT set width/height - let the framework determine content-only dimensions
                // The framework will automatically crop to content area and provide correct dimensions
                NSLog(@"🎯 Phase E.3: Allowing framework to determine content-only dimensions automatically");
                
                // DO NOT set sourceRect - explicitly ignored for desktop-independent windows
                // config.sourceRect is meaningless and ignored for initWithDesktopIndependentWindow
                
                config.pixelFormat = kCVPixelFormatType_32BGRA;
                config.showsCursor = YES;
                config.capturesAudio = NO;
                config.sampleRate = 60;
                config.minimumFrameInterval = CMTimeMake(1, 60);
                
                // Allow the system to optimize for content-only capture
                config.scalesToFit = NO;
                config.preservesAspectRatio = YES;
                
                NSLog(@"🎯 Phase E.3: Using Apple's recommended desktop-independent window capture - no manual cropping needed");
                
                // Create the stream
                scStream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:screenCaptureKitDelegate];
                
                if (!screenCaptureKitDelegate) {
                    screenCaptureKitDelegate = [[ScreenCaptureKitDelegate alloc] init];
                }
                
                // Add output for receiving frames
                NSError *outputError = nil;
                [scStream addStreamOutput:screenCaptureKitDelegate type:SCStreamOutputTypeScreen sampleHandlerQueue:captureQueue error:&outputError];
                if (outputError) {
                    NSLog(@"❌ Failed to add stream output: %@", outputError.localizedDescription);
                    return;
                }
                
                // Start capture
                [scStream startCaptureWithCompletionHandler:^(NSError * _Nullable error) {
                    if (error) {
                        NSLog(@"❌ Failed to start ScreenCaptureKit content-only capture: %@", error.localizedDescription);
                    } else {
                        NSLog(@"✅ Phase E.3: ScreenCaptureKit content-only capture started for window %u! 🎉", windowID);
                    }
                }];
            }];
            
            return true;  // Return immediately, actual success/failure handled in completion handlers
        } else {
            NSLog(@"❌ ScreenCaptureKit not available on this macOS version (requires 12.3+)");
            return false;
        }
    }
}

// Check if we have a recent frame available
bool screencapture_has_frame(void) {
    os_unfair_lock_lock(&frameLock);
    bool hasFrame = (latestFrame != NULL);
    os_unfair_lock_unlock(&frameLock);
    return hasFrame;
}