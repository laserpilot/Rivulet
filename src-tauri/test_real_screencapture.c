// Real screen capture test - captures actual moving desktop content
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mach/mach_time.h>

// Include our enhanced Syphon bridge functions
typedef struct SyphonServerState SyphonServerState;
extern SyphonServerState* syphon_server_create(const char* name, const void* ctx);
extern bool syphon_server_publish_screen_capture(SyphonServerState* state);
extern bool syphon_server_start_screen_capture(void);
extern void syphon_server_stop_screen_capture(void);
extern bool syphon_server_has_screen_frame(void);
extern void syphon_server_stop(SyphonServerState* state);

// Add OpenGL context creation
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

// High-precision timing
static mach_timebase_info_data_t timebase_info;
static int timebase_initialized = 0;

uint64_t get_nanoseconds() {
    if (!timebase_initialized) {
        mach_timebase_info(&timebase_info);
        timebase_initialized = 1;
    }
    uint64_t mach_time = mach_absolute_time();
    return (mach_time * timebase_info.numer) / timebase_info.denom;
}

void test_real_screen_capture(SyphonServerState* server, int duration_seconds) {
    printf("\n🖥️  TESTING REAL SCREEN CAPTURE with Zero-Copy IOSurface\n");
    printf("Duration: %d seconds\n", duration_seconds);
    printf("💡 Move your mouse or windows to see live desktop content!\n\n");
    
    uint64_t start_time = get_nanoseconds();
    uint64_t total_frame_time = 0;
    int successful_frames = 0;
    int failed_frames = 0;
    
    uint64_t end_time = start_time + (duration_seconds * 1000000000ULL); // Convert to nanoseconds
    
    while (get_nanoseconds() < end_time) {
        if (syphon_server_has_screen_frame()) {
            uint64_t frame_start = get_nanoseconds();
            
            bool success = syphon_server_publish_screen_capture(server);
            
            uint64_t frame_end = get_nanoseconds();
            
            if (success) {
                total_frame_time += (frame_end - frame_start);
                successful_frames++;
                
                // Progress indicator every 2 seconds
                if (successful_frames % 120 == 0) {
                    double elapsed_sec = (frame_end - start_time) / 1000000000.0;
                    double current_fps = successful_frames / elapsed_sec;
                    printf("📊 %d frames: %.1f FPS (real screen content)\n", successful_frames, current_fps);
                }
            } else {
                failed_frames++;
            }
        }
        
        // Target ~60 FPS
        usleep(16667); // ~16.67ms
    }
    
    uint64_t actual_end_time = get_nanoseconds();
    double total_time_sec = (actual_end_time - start_time) / 1000000000.0;
    double actual_fps = successful_frames / total_time_sec;
    double avg_frame_time_ms = successful_frames > 0 ? (total_frame_time / successful_frames) / 1000000.0 : 0;
    
    printf("\n✅ REAL SCREEN CAPTURE Results:\n");
    printf("   Successful frames: %d\n", successful_frames);
    printf("   Failed frames: %d\n", failed_frames);
    printf("   Average frame time: %.3f ms\n", avg_frame_time_ms);
    printf("   Actual FPS achieved: %.1f FPS\n", actual_fps);
    printf("   Total time: %.1f seconds\n", total_time_sec);
    printf("   Success rate: %.1f%%\n", successful_frames * 100.0 / (successful_frames + failed_frames));
}

int main() {
    printf("🖥️  REAL SCREEN CAPTURE TEST - Live Desktop Content\n");
    printf("====================================================\n");
    printf("This test captures actual desktop content using AVFoundation\n");
    printf("and publishes it via zero-copy IOSurface to Syphon.\n\n");
    
    printf("👀 SETUP INSTRUCTIONS:\n");
    printf("1. Open a Syphon client (like Syphon Recorder)\n");
    printf("2. Look for 'Live Desktop Capture' server\n");
    printf("3. You should see your actual desktop content\n");
    printf("4. Move windows/mouse to see live updates\n");
    printf("5. Watch for smooth real-time capture\n\n");
    
    printf("🔐 PERMISSION NOTE:\n");
    printf("macOS may ask for Screen Recording permission.\n");
    printf("Grant permission and restart this test if needed.\n\n");
    
    printf("Creating OpenGL context...\n");
    
    // Create OpenGL context for Syphon
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated,
        kCGLPFANoRecovery,
        kCGLPFAColorSize, 24,
        kCGLPFAAlphaSize, 8,
        kCGLPFADepthSize, 16,
        0
    };
    
    CGLPixelFormatObj pixelFormat;
    GLint numPixelFormats;
    CGLError error = CGLChoosePixelFormat(attrs, &pixelFormat, &numPixelFormats);
    if (error != kCGLNoError) {
        printf("❌ Failed to choose OpenGL pixel format: %d\n", error);
        return 1;
    }
    
    CGLContextObj glContext;
    error = CGLCreateContext(pixelFormat, NULL, &glContext);
    if (error != kCGLNoError) {
        printf("❌ Failed to create OpenGL context: %d\n", error);
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    
    // Make context current
    error = CGLSetCurrentContext(glContext);
    if (error != kCGLNoError) {
        printf("❌ Failed to set OpenGL context current: %d\n", error);
        CGLDestroyContext(glContext);
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    
    printf("✅ OpenGL context created and set current\n");
    
    printf("Creating Syphon server...\n");
    SyphonServerState* server = syphon_server_create("Live Desktop Capture", glContext);
    if (!server) {
        printf("❌ Failed to create Syphon server\n");
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(glContext);
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    printf("✅ Syphon server 'Live Desktop Capture' created\n");
    
    printf("Initializing screen capture...\n");
    if (!syphon_server_start_screen_capture()) {
        printf("❌ Failed to initialize screen capture\n");
        printf("💡 Check Screen Recording permissions in System Preferences > Privacy\n");
        syphon_server_stop(server);
        return 1;
    }
    printf("✅ Screen capture initialized\n");
    
    printf("📺 Server should now be visible in Syphon clients!\n");
    printf("⏱️  Starting capture in 3 seconds... (time to open your Syphon client)\n");
    sleep(3);
    
    // Wait for first frame
    printf("⏳ Waiting for first screen frame...\n");
    int wait_count = 0;
    while (!syphon_server_has_screen_frame() && wait_count < 50) {
        usleep(100000); // 100ms
        wait_count++;
    }
    
    if (!syphon_server_has_screen_frame()) {
        printf("⚠️  No screen frames received after 5 seconds\n");
        printf("💡 This may indicate permission issues or screen capture problems\n");
    } else {
        printf("✅ Screen frames detected - starting live capture test!\n");
    }
    
    // Run real screen capture test
    test_real_screen_capture(server, 10); // 10 seconds of live capture
    
    printf("\n💡 EXPECTED RESULTS:\n");
    printf("- You should have seen live desktop content in your Syphon client\n");
    printf("- Moving windows/mouse should update in real-time\n");
    printf("- Zero-copy IOSurface should provide smooth performance\n");
    printf("- Frame rate should be close to 60 FPS\n\n");
    
    printf("🧹 Cleaning up...\n");
    syphon_server_stop_screen_capture();
    syphon_server_stop(server);
    
    // Clean up OpenGL context
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(glContext);
    CGLDestroyPixelFormat(pixelFormat);
    
    printf("✅ Real screen capture test completed\n");
    
    return 0;
}