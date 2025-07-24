// Zero-Copy ScreenCaptureKit Frontend for Ultimate Performance
console.log('🚀 Loading Zero-Copy Syphon Frontend...');
const { invoke } = window.__TAURI__.tauri;

// Application state
let isInitialized = false;
let isCapturing = false;
let frameCount = 0;
let captureMode = 'Zero-Copy';

// Performance monitoring
let lastFpsTime = Date.now();
let fpsFrameCount = 0;
let currentFps = 0;
let processingTimer = null;

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    console.log('🎯 Zero-Copy Frontend initialized');
    updateStatus('Ready for zero-copy initialization', false);
    updateUI();
});

// Initialize traditional video sharing (Syphon only)
async function initializeVideoSharing() {
    console.log('🔧 Initializing Syphon server...');
    updateStatus('Initializing Syphon server...', false);
    
    try {
        const response = await invoke('initialize_video_sharing');
        console.log('✅ Syphon server initialized:', response);
        
        isInitialized = true;
        updateStatus('Syphon server ready', true);
        updateUI();
        
    } catch (error) {
        console.error('❌ Failed to initialize Syphon server:', error);
        updateStatus(`Error: ${error}`, false);
    }
}

// Initialize window self-capture pipeline (NEW!)
async function initializeWindowCapture() {
    if (!isInitialized) {
        updateStatus('Please initialize Syphon server first', false);
        return;
    }

    console.log('🎯 Initializing window self-capture...');
    updateStatus('Setting up window self-capture...', false);
    
    try {
        // Step 1: Initialize window capture system
        const initResponse = await invoke('initialize_window_capture');
        console.log('🚀 Window capture initialized:', initResponse);
        
        updateStatus('Starting continuous window capture...', false);
        
        // Step 2: Start continuous capture publishing
        const startResponse = await invoke('start_continuous_screen_capture');
        console.log('🎬 Continuous window capture started:', startResponse);
        
        isCapturing = true;
        captureMode = 'Window Self-Capture (Zero-Copy)';
        updateStatus('Window self-capture active! (Browser → Syphon)', true);
        updateUI();
        
        // Monitor performance
        startPerformanceMonitoring();
        
    } catch (error) {
        console.error('❌ Failed to initialize window capture:', error);
        updateStatus(`Window capture error: ${error}`, false);
    }
}

// Initialize zero-copy capture pipeline (Desktop)
async function initializeZeroCopyCapture() {
    if (!isInitialized) {
        updateStatus('Please initialize Syphon server first', false);
        return;
    }

    console.log('🖥️ Initializing desktop screen capture...');
    updateStatus('Requesting Screen Recording permission...', false);
    
    try {
        // Step 1: Initialize screen capture system
        const initResponse = await invoke('initialize_screen_capture');
        console.log('🚀 Desktop capture initialized:', initResponse);
        
        updateStatus('Starting continuous capture...', false);
        
        // Step 2: Start continuous screen capture publishing
        const startResponse = await invoke('start_continuous_screen_capture');
        console.log('🎬 Continuous capture started:', startResponse);
        
        isCapturing = true;
        captureMode = 'Desktop Capture (Zero-Copy)';
        updateStatus('Desktop capture active! (60+ FPS)', true);
        updateUI();
        
        // Monitor performance (no longer need to manually process frames)
        startPerformanceMonitoring();
        
    } catch (error) {
        console.error('❌ Failed to initialize zero-copy capture:', error);
        updateStatus(`Zero-copy error: ${error}`, false);
    }
}

// Start performance monitoring (background thread handles the actual publishing)
function startPerformanceMonitoring() {
    if (processingTimer) {
        clearInterval(processingTimer);
    }
    
    console.log('📊 Starting performance monitoring...');
    
    // Monitor performance stats every 500ms (less frequent since capture is automatic)
    processingTimer = setInterval(async () => {
        if (!isCapturing) {
            stopPerformanceMonitoring();
            return;
        }
        
        try {
            const response = await invoke('get_video_status');
            
            if (response.success && response.frame_count > frameCount) {
                frameCount = response.frame_count;
                updatePerformanceStats();
            }
            
        } catch (error) {
            console.error('❌ Performance monitoring error:', error);
            // Don't stop on single errors, just log them
        }
    }, 500); // Monitor every 500ms
}

// Stop performance monitoring
function stopPerformanceMonitoring() {
    if (processingTimer) {
        clearInterval(processingTimer);
        processingTimer = null;
        console.log('🛑 Performance monitoring stopped');
    }
}

// Stop capture - now properly stops the backend screen capture
async function stopCapture() {
    console.log('🛑 Stopping screen capture...');
    
    try {
        const response = await invoke('stop_screen_capture');
        console.log('✅ Screen capture stopped:', response);
        
        isCapturing = false;
        stopPerformanceMonitoring();
        updateStatus('Screen capture stopped', isInitialized);
        updateUI();
        
    } catch (error) {
        console.error('❌ Failed to stop capture:', error);
        updateStatus(`Stop error: ${error}`, false);
    }
}

// Update performance statistics
function updatePerformanceStats() {
    fpsFrameCount++;
    const now = Date.now();
    
    if (now - lastFpsTime >= 1000) {
        currentFps = Math.round(fpsFrameCount * 1000 / (now - lastFpsTime));
        fpsFrameCount = 0;
        lastFpsTime = now;
        
        // Update UI with performance stats
        const statsElement = document.getElementById('statsText');
        if (statsElement) {
            statsElement.textContent = 
                `Frames: ${frameCount} | FPS: ${currentFps} | Mode: ${captureMode}`;
        }
    }
}

// Update UI button states
function updateUI() {
    const initBtn = document.getElementById('initBtn');
    const windowCaptureBtn = document.getElementById('windowCaptureBtn');
    const zeroCopyBtn = document.getElementById('zeroCopyBtn');
    const stopBtn = document.getElementById('stopBtn');
    
    if (initBtn) initBtn.disabled = isInitialized;
    if (windowCaptureBtn) windowCaptureBtn.disabled = !isInitialized || isCapturing;
    if (zeroCopyBtn) zeroCopyBtn.disabled = !isInitialized || isCapturing;
    if (stopBtn) stopBtn.disabled = !isCapturing;
}

// Update status display
function updateStatus(message, active) {
    const indicator = document.querySelector('.status-indicator');
    const statusText = document.getElementById('statusText');
    
    if (indicator) {
        indicator.className = `status-indicator ${active ? 'status-active' : 'status-inactive'}`;
    }
    if (statusText) {
        statusText.textContent = message;
    }
}

// Check status periodically
setInterval(async () => {
    if (isInitialized && !isCapturing) {
        try {
            const status = await invoke('get_video_status');
            updateStatus(`${status.message} (${status.frame_count} frames)`, status.success);
        } catch (error) {
            console.error('Error checking status:', error);
        }
    }
}, 2000);

console.log('✅ Zero-Copy Frontend ready for ultimate performance!');