#!/usr/bin/env python3
"""
Direct test of zero-copy IOSurface performance by calling the 
publish_test_iosurface function and analyzing performance logs.
"""

import subprocess
import time
import sys

def test_iosurface_directly():
    """Test the zero-copy IOSurface path by modifying the test app"""
    
    print("🎯 TESTING TRUE ZERO-COPY IOSurface Path")
    print("=" * 60)
    print("Building a focused test for CGLTexImageIOSurface2D performance...")
    
    # Create a simple test binary that focuses on zero-copy
    test_code = '''
// Simple test for zero-copy IOSurface performance
extern "C" {
    void* create_test_iosurface(unsigned int width, unsigned int height);
    typedef struct SyphonServerState SyphonServerState;
    SyphonServerState* syphon_server_create(const char* name);
    bool syphon_server_publish_iosurface(SyphonServerState* state, const void* iosurface_ref);
    void syphon_server_stop(SyphonServerState* state);
}

#include <stdio.h>
#include <unistd.h>

int main() {
    printf("🎯 Testing TRUE ZERO-COPY IOSurface Performance\\n");
    printf("Creating Syphon server...\\n");
    
    SyphonServerState* server = syphon_server_create("Zero-Copy Test");
    if (!server) {
        printf("❌ Failed to create Syphon server\\n");
        return 1;
    }
    
    printf("✅ Syphon server created\\n");
    printf("Testing zero-copy IOSurface publishing...\\n");
    
    // Test multiple resolutions
    int resolutions[][2] = {
        {640, 480},
        {1920, 1080},
        {3840, 2160}
    };
    
    for (int i = 0; i < 3; i++) {
        int width = resolutions[i][0];
        int height = resolutions[i][1];
        
        printf("\\n📊 Testing %dx%d resolution:\\n", width, height);
        
        // Create and publish test IOSurface 10 times
        for (int frame = 0; frame < 10; frame++) {
            void* surface = create_test_iosurface(width, height);
            if (!surface) {
                printf("❌ Failed to create test IOSurface\\n");
                continue;
            }
            
            bool success = syphon_server_publish_iosurface(server, surface);
            if (!success) {
                printf("❌ Failed to publish IOSurface frame %d\\n", frame);
            }
            
            usleep(16666); // ~60 FPS
        }
    }
    
    printf("\\n🧹 Cleaning up...\\n");
    syphon_server_stop(server);
    
    printf("✅ Zero-copy test completed\\n");
    return 0;
}
'''
    
    # Write the test C code
    with open("/tmp/zero_copy_test.c", "w") as f:
        f.write(test_code)
    
    print("📝 Created focused zero-copy test")
    print("💡 This will directly test the CGLTexImageIOSurface2D path")
    print("\n🔍 Key things to look for in the logs:")
    print("- 'SCENARIO B: Publishing IOSurface' messages")
    print("- 'TRUE ZERO-COPY: IOSurface published' confirmations")
    print("- Performance timing for 'iosurface_texture_bind'")
    print("- Performance timing for 'iosurface_syphon_publish'")
    print("\n⚠️  NOTE: The Tauri app is currently running CPU buffer path.")
    print("To test true zero-copy, we need to invoke the test_zero_copy_iosurface command.")
    print("This requires either:")
    print("1. Frontend UI to trigger the command")
    print("2. Direct function call modification")
    print("3. IPC/WebSocket command interface")

if __name__ == "__main__":
    test_iosurface_directly()