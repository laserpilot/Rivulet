#!/usr/bin/env python3
"""
Test script to compare CPU buffer vs Zero-Copy IOSurface performance
by invoking Tauri commands and analyzing the performance logs.
"""

import json
import subprocess
import time
import sys
import signal
import threading
from pathlib import Path

def run_tauri_app():
    """Run the Tauri application in background"""
    app_path = Path(__file__).parent / "target" / "release" / "electron-video-share"
    return subprocess.Popen([str(app_path)], 
                          stdout=subprocess.PIPE, 
                          stderr=subprocess.STDOUT,
                          text=True,
                          bufsize=1)

def test_zero_copy_command():
    """Test the zero-copy IOSurface command via command line"""
    print("🎯 Testing TRUE ZERO-COPY IOSurface Performance")
    print("=" * 60)
    
    # Start the Tauri application
    print("Starting Tauri application...")
    proc = run_tauri_app()
    
    try:
        # Wait for initialization
        time.sleep(2)
        
        # The app automatically starts publishing CPU frames
        # Look for zero-copy test in the output logs
        print("\n📊 ANALYSIS: Monitoring performance logs...")
        print("Looking for CPU→GPU vs IOSurface performance comparison:")
        
        cpu_times = []
        syphon_times = []
        total_times = []
        
        # Read output for a limited time
        start_time = time.time()
        while time.time() - start_time < 10:  # Monitor for 10 seconds
            line = proc.stdout.readline()
            if line:
                line = line.strip()
                print(line)
                
                # Parse performance metrics
                if "cpu_to_gpu_upload took" in line:
                    try:
                        time_ms = float(line.split("took ")[1].split(" ms")[0])
                        cpu_times.append(time_ms)
                    except:
                        pass
                        
                elif "syphon_publish took" in line:
                    try:
                        time_ms = float(line.split("took ")[1].split(" ms")[0])
                        syphon_times.append(time_ms)
                    except:
                        pass
                        
                elif "total_frame took" in line:
                    try:
                        time_ms = float(line.split("took ")[1].split(" ms")[0])
                        total_times.append(time_ms)
                    except:
                        pass
            else:
                time.sleep(0.01)
        
        # Analyze performance
        print("\n" + "=" * 60)
        print("📈 PERFORMANCE ANALYSIS RESULTS:")
        print("=" * 60)
        
        if cpu_times:
            avg_cpu = sum(cpu_times) / len(cpu_times)
            print(f"CPU→GPU Upload Time: {avg_cpu:.3f} ms (avg of {len(cpu_times)} samples)")
            
        if syphon_times:
            avg_syphon = sum(syphon_times) / len(syphon_times)
            print(f"Syphon Publish Time: {avg_syphon:.3f} ms (avg of {len(syphon_times)} samples)")
            
        if total_times:
            avg_total = sum(total_times) / len(total_times)
            max_fps = 1000.0 / avg_total
            print(f"Total Frame Time: {avg_total:.3f} ms (avg of {len(total_times)} samples)")
            print(f"Theoretical Max FPS: {max_fps:.1f} FPS")
            
        print("\n🎯 NEXT: Need to test zero-copy IOSurface path")
        print("The application is currently running the CPU buffer path.")
        print("To test true zero-copy, we need to trigger the test_zero_copy_iosurface command.")
        
    finally:
        # Clean shutdown
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

if __name__ == "__main__":
    test_zero_copy_command()