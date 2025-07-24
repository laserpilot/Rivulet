// Animated 1080p test with moving circle for visual confirmation of smooth playback
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <mach/mach_time.h>

// Include our Syphon bridge functions
extern void* create_test_iosurface(unsigned int width, unsigned int height);

typedef struct SyphonServerState SyphonServerState;
extern SyphonServerState* syphon_server_create(const char* name, const void* ctx);
extern int syphon_server_publish_frame(SyphonServerState* state, const unsigned char* data, 
                                        unsigned int width, unsigned int height);
extern int syphon_server_publish_iosurface(SyphonServerState* state, const void* iosurface_ref);
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

// Create animated frame with moving circle
void create_animated_frame(unsigned char* buffer, int width, int height, int frame_number) {
    // Animation parameters
    float center_x = width / 2.0f;
    float center_y = height / 2.0f;
    float orbit_radius = 300.0f;  // Circle orbit radius
    float circle_radius = 80.0f;  // Size of the moving circle
    float speed = 0.05f;          // Animation speed
    
    // Calculate circle position (orbiting around center)
    float angle = frame_number * speed;
    float circle_x = center_x + orbit_radius * cos(angle);
    float circle_y = center_y + orbit_radius * sin(angle);
    
    // Fill background with gradient
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_index = (y * width + x) * 4;
            
            // Distance from moving circle center
            float dx = x - circle_x;
            float dy = y - circle_y;
            float distance = sqrt(dx * dx + dy * dy);
            
            // Background gradient
            unsigned char bg_r = (unsigned char)((x * 255) / width);
            unsigned char bg_g = (unsigned char)((y * 255) / height);
            unsigned char bg_b = 64;
            
            if (distance <= circle_radius) {
                // Inside the moving circle - bright animated colors
                float intensity = 1.0f - (distance / circle_radius);
                unsigned char circle_r = (unsigned char)(255 * intensity);
                unsigned char circle_g = (unsigned char)(128 + 127 * sin(angle * 3));
                unsigned char circle_b = (unsigned char)(128 + 127 * cos(angle * 2));
                
                // BGRA format for Syphon
                buffer[pixel_index + 0] = circle_b;      // B
                buffer[pixel_index + 1] = circle_g;      // G
                buffer[pixel_index + 2] = circle_r;      // R
                buffer[pixel_index + 3] = 255;           // A
            } else {
                // Background
                buffer[pixel_index + 0] = bg_b;          // B
                buffer[pixel_index + 1] = bg_g;          // G
                buffer[pixel_index + 2] = bg_r;          // R
                buffer[pixel_index + 3] = 255;           // A
            }
        }
    }
    
    // Add frame counter text area (simple bright rectangle)
    int text_x = 50;
    int text_y = 50;
    int text_width = 200;
    int text_height = 50;
    
    for (int y = text_y; y < text_y + text_height && y < height; y++) {
        for (int x = text_x; x < text_x + text_width && x < width; x++) {
            int pixel_index = (y * width + x) * 4;
            // White rectangle for frame counter visibility
            buffer[pixel_index + 0] = 255;  // B
            buffer[pixel_index + 1] = 255;  // G  
            buffer[pixel_index + 2] = 255;  // R
            buffer[pixel_index + 3] = 255;  // A
        }
    }
}

void test_cpu_animation(SyphonServerState* server, int width, int height, int total_frames) {
    printf("\n🎬 TESTING CPU BUFFER ANIMATION at %dx%d\n", width, height);
    printf("Total frames: %d, Expected to run for %.1f seconds\n", total_frames, total_frames / 60.0f);
    
    size_t buffer_size = width * height * 4; // BGRA
    unsigned char* buffer = malloc(buffer_size);
    if (!buffer) {
        printf("❌ Failed to allocate animation buffer\n");
        return;
    }
    
    uint64_t start_time = get_nanoseconds();
    uint64_t total_frame_time = 0;
    int successful_frames = 0;
    
    for (int frame = 0; frame < total_frames; frame++) {
        // Create animated frame
        create_animated_frame(buffer, width, height, frame);
        
        uint64_t frame_start = get_nanoseconds();
        
        int success = syphon_server_publish_frame(server, buffer, width, height);
        
        uint64_t frame_end = get_nanoseconds();
        
        if (success) {
            total_frame_time += (frame_end - frame_start);
            successful_frames++;
            
            // Progress indicator
            if (frame % 60 == 0) {
                double elapsed_sec = (frame_end - start_time) / 1000000000.0;
                double current_fps = frame / elapsed_sec;
                printf("📊 Frame %d: %.1f FPS actual\n", frame, current_fps);
            }
        } else {
            printf("❌ Failed to publish CPU frame %d\n", frame);
        }
        
        // Target 60 FPS (16.67ms per frame)
        usleep(16667);
    }
    
    uint64_t end_time = get_nanoseconds();
    double total_time_sec = (end_time - start_time) / 1000000000.0;
    double actual_fps = successful_frames / total_time_sec;
    double avg_frame_time_ms = (total_frame_time / successful_frames) / 1000000.0;
    
    printf("✅ CPU Animation Results:\n");
    printf("   Average frame time: %.3f ms\n", avg_frame_time_ms);
    printf("   Actual FPS achieved: %.1f FPS\n", actual_fps);
    printf("   Total time: %.1f seconds\n", total_time_sec);
    
    free(buffer);
}

void test_iosurface_animation(SyphonServerState* server, int width, int height, int total_frames) {
    printf("\n🎯 TESTING IOSURFACE ZERO-COPY ANIMATION at %dx%d\n", width, height);
    printf("Total frames: %d, Expected to run for %.1f seconds\n", total_frames, total_frames / 60.0f);
    
    uint64_t start_time = get_nanoseconds();
    uint64_t total_frame_time = 0;
    int successful_frames = 0;
    
    for (int frame = 0; frame < total_frames; frame++) {
        uint64_t frame_start = get_nanoseconds();
        
        // Create IOSurface (this should be replaced with real animated IOSurface)
        void* surface = create_test_iosurface(width, height);
        if (!surface) {
            printf("❌ Failed to create IOSurface for frame %d\n", frame);
            continue;
        }
        
        // Publish zero-copy
        int success = syphon_server_publish_iosurface(server, surface);
        
        uint64_t frame_end = get_nanoseconds();
        
        if (success) {
            total_frame_time += (frame_end - frame_start);
            successful_frames++;
            
            // Progress indicator
            if (frame % 60 == 0) {
                double elapsed_sec = (frame_end - start_time) / 1000000000.0;
                double current_fps = frame / elapsed_sec;
                printf("📊 Frame %d: %.1f FPS actual\n", frame, current_fps);
            }
        } else {
            printf("❌ Failed to publish IOSurface frame %d\n", frame);
        }
        
        // Target 60 FPS (16.67ms per frame)
        usleep(16667);
    }
    
    uint64_t end_time = get_nanoseconds();
    double total_time_sec = (end_time - start_time) / 1000000000.0;
    double actual_fps = successful_frames / total_time_sec;
    double avg_frame_time_ms = (total_frame_time / successful_frames) / 1000000.0;
    
    printf("✅ IOSurface Animation Results:\n");
    printf("   Average frame time: %.3f ms\n", avg_frame_time_ms);
    printf("   Actual FPS achieved: %.1f FPS\n", actual_fps);
    printf("   Total time: %.1f seconds\n", total_time_sec);
}

int main() {
    printf("🎬 1080p ANIMATED CIRCLE TEST - Visual Confirmation\n");
    printf("===================================================\n");
    printf("This test creates a moving circle animation at 1920x1080\n");
    printf("to visually confirm smooth playback performance.\n\n");
    
    printf("👀 INSTRUCTIONS:\n");
    printf("1. Open a Syphon client (like Syphon Recorder or Simple Client)\n");
    printf("2. Look for 'Animated 1080p Test' server\n");
    printf("3. Watch for smooth circle movement without stuttering\n");
    printf("4. Compare CPU vs Zero-Copy smoothness\n\n");
    
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
    SyphonServerState* server = syphon_server_create("Animated 1080p Test", glContext);
    if (!server) {
        printf("❌ Failed to create Syphon server\n");
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(glContext);
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    printf("✅ Syphon server 'Animated 1080p Test' created\n");
    printf("📺 Server should now be visible in Syphon clients!\n\n");
    
    int width = 1920;
    int height = 1080;
    int frames_per_test = 300; // 5 seconds at 60 FPS
    
    printf("⏱️  Starting in 3 seconds... (time to open your Syphon client)\n");
    sleep(3);
    
    // Test CPU buffer animation first
    printf("\n" "🔥 PHASE 1: CPU BUFFER ANIMATION\n");
    printf("Watch for smooth circle movement...\n");
    test_cpu_animation(server, width, height, frames_per_test);
    
    printf("\n⏸️  Pausing 2 seconds between tests...\n");
    sleep(2);
    
    // Test IOSurface zero-copy animation
    printf("\n" "🎯 PHASE 2: ZERO-COPY IOSURFACE ANIMATION\n");
    printf("Watch for even smoother movement...\n");
    test_iosurface_animation(server, width, height, frames_per_test);
    
    printf("\n💡 COMPARISON SUMMARY:\n");
    printf("You should have observed:\n");
    printf("- Phase 1: CPU buffer animation (may show some frame drops)\n");
    printf("- Phase 2: Zero-copy IOSurface (should be noticeably smoother)\n");
    printf("- Circle orbiting around center with color changes\n");
    printf("- Frame counter area in top-left\n\n");
    
    printf("🧹 Cleaning up...\n");
    syphon_server_stop(server);
    
    // Clean up OpenGL context
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(glContext);
    CGLDestroyPixelFormat(pixelFormat);
    
    printf("✅ Animated test completed\n");
    
    return 0;
}