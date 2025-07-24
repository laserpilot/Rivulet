// Direct test of zero-copy IOSurface vs CPU buffer performance
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

void test_cpu_buffer_performance(SyphonServerState* server, int width, int height, int frames) {
    printf("\n🔥 TESTING CPU BUFFER PATH (Scenario A)\n");
    printf("Resolution: %dx%d, Frames: %d\n", width, height, frames);
    
    // Create test buffer
    size_t buffer_size = width * height * 4; // RGBA
    unsigned char* buffer = malloc(buffer_size);
    if (!buffer) {
        printf("❌ Failed to allocate test buffer\n");
        return;
    }
    
    // Fill with test pattern
    for (int i = 0; i < buffer_size; i += 4) {
        buffer[i] = (i / 4) % 256;     // B
        buffer[i+1] = ((i / 4) / 256) % 256; // G
        buffer[i+2] = 128;             // R
        buffer[i+3] = 255;             // A
    }
    
    uint64_t total_time = 0;
    int successful_frames = 0;
    
    for (int frame = 0; frame < frames; frame++) {
        uint64_t start = get_nanoseconds();
        
        int success = syphon_server_publish_frame(server, buffer, width, height);
        
        uint64_t end = get_nanoseconds();
        
        if (success) {
            total_time += (end - start);
            successful_frames++;
        } else {
            printf("❌ Failed to publish CPU frame %d\n", frame);
        }
    }
    
    if (successful_frames > 0) {
        double avg_time_ms = (total_time / successful_frames) / 1000000.0;
        double avg_fps = 1000.0 / avg_time_ms;
        printf("✅ CPU Buffer Results: %.3f ms avg, %.1f FPS max\n", avg_time_ms, avg_fps);
    }
    
    free(buffer);
}

void test_iosurface_performance(SyphonServerState* server, int width, int height, int frames) {
    printf("\n🎯 TESTING IOSURFACE ZERO-COPY PATH (Scenario B)\n");
    printf("Resolution: %dx%d, Frames: %d\n", width, height, frames);
    
    uint64_t total_time = 0;
    int successful_frames = 0;
    
    for (int frame = 0; frame < frames; frame++) {
        uint64_t start = get_nanoseconds();
        
        // Create IOSurface
        void* surface = create_test_iosurface(width, height);
        if (!surface) {
            printf("❌ Failed to create IOSurface for frame %d\n", frame);
            continue;
        }
        
        // Publish zero-copy
        int success = syphon_server_publish_iosurface(server, surface);
        
        uint64_t end = get_nanoseconds();
        
        if (success) {
            total_time += (end - start);
            successful_frames++;
        } else {
            printf("❌ Failed to publish IOSurface frame %d\n", frame);
        }
        
        // IOSurface cleanup handled by system
    }
    
    if (successful_frames > 0) {
        double avg_time_ms = (total_time / successful_frames) / 1000000.0;
        double avg_fps = 1000.0 / avg_time_ms;
        printf("✅ IOSurface Results: %.3f ms avg, %.1f FPS max\n", avg_time_ms, avg_fps);
    }
}

int main() {
    printf("🚀 ZERO-COPY vs CPU BUFFER PERFORMANCE COMPARISON\n");
    printf("==================================================\n");
    
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
    SyphonServerState* server = syphon_server_create("Performance Test", glContext);
    if (!server) {
        printf("❌ Failed to create Syphon server\n");
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(glContext);
        CGLDestroyPixelFormat(pixelFormat);
        return 1;
    }
    printf("✅ Syphon server created\n");
    
    // Test different resolutions
    int test_cases[][3] = {
        {640, 480, 30},      // 30 frames at 640x480
        {1920, 1080, 20},    // 20 frames at 1080p  
        {3840, 2160, 10}     // 10 frames at 4K
    };
    
    for (int i = 0; i < 3; i++) {
        int width = test_cases[i][0];
        int height = test_cases[i][1];
        int frames = test_cases[i][2];
        
        printf("\n==================================================\n");
        printf("TEST CASE %d: %dx%d Resolution\n", i+1, width, height);
        printf("==================================================\n");
        
        // Test CPU buffer path
        test_cpu_buffer_performance(server, width, height, frames);
        
        // Wait between tests
        usleep(100000); // 100ms
        
        // Test IOSurface zero-copy path
        test_iosurface_performance(server, width, height, frames);
        
        printf("\n💡 COMPARISON: IOSurface should be significantly faster than CPU buffer\n");
    }
    
    printf("\n🧹 Cleaning up...\n");
    syphon_server_stop(server);
    
    // Clean up OpenGL context
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(glContext);
    CGLDestroyPixelFormat(pixelFormat);
    
    printf("✅ Performance test completed\n");
    
    return 0;
}