const { platform } = require('os');
const path = require('path');

// Platform detection
const isWindows = platform() === 'win32';
const isMacOS = platform() === 'darwin';

if (!isWindows && !isMacOS) {
  throw new Error('electron-video-share only supports Windows and macOS');
}

// Load the native module
let nativeModule;
try {
  nativeModule = require('./build/Release/electron-video-share.node');
} catch (err) {
  try {
    nativeModule = require('./build/Debug/electron-video-share.node');
  } catch (debugErr) {
    throw new Error(`Failed to load native module: ${err.message}`);
  }
}

/**
 * Cross-platform video output class
 * Uses Spout on Windows and Syphon on macOS
 */
class VideoOutput {
  constructor(name, options = {}) {
    if (typeof name !== 'string') {
      throw new TypeError('Name must be a string');
    }
    
    this.name = name;
    this.platform = platform();
    this.options = options;
    
    // Create platform-specific implementation
    if (isWindows) {
      this._impl = new nativeModule.SpoutOutput(name, options);
    } else if (isMacOS) {
      this._impl = nativeModule.createSyphonOutput(name, options);
    }
  }

  /**
   * Update with frame data (bitmap)
   * @param {Buffer} bitmap - Frame bitmap data
   * @param {Object} size - Frame size {width, height}
   */
  updateFrame(bitmap, size) {
    if (!Buffer.isBuffer(bitmap)) {
      throw new TypeError('Bitmap must be a Buffer');
    }
    if (!size || typeof size.width !== 'number' || typeof size.height !== 'number') {
      throw new TypeError('Size must be an object with width and height properties');
    }
    
    // Call the native updateFrame function with the implementation as context
    if (isMacOS) {
      return nativeModule.updateFrame ? nativeModule.updateFrame(this._impl, bitmap, size) : false;
    } else {
      return this._impl.updateFrame(bitmap, size);
    }
  }

  /**
   * Update with texture data (when offscreenUseSharedTexture is true)
   * @param {*} texture - Platform-specific texture handle
   */
  updateTexture(texture) {
    if (!texture) {
      throw new TypeError('Texture cannot be null or undefined');
    }
    
    return this._impl.updateTexture(texture);
  }

  /**
   * Get output name
   * @returns {string} Output name
   */
  getName() {
    return this.name;
  }

  /**
   * Get platform information
   * @returns {Object} Platform info
   */
  getPlatformInfo() {
    return {
      platform: this.platform,
      framework: isWindows ? 'Spout' : 'Syphon',
      version: '2.0.0'
    };
  }

  /**
   * Check if clients are connected
   * @returns {boolean} True if clients are connected
   */
  hasClients() {
    return this._impl.hasClients ? this._impl.hasClients() : false;
  }

  /**
   * Stop the output
   */
  stop() {
    if (this._impl && this._impl.stop) {
      this._impl.stop();
    }
  }
}

// Export unified API
module.exports = {
  VideoOutput,
  // Legacy compatibility
  SpoutOutput: VideoOutput,
  SyphonOutput: VideoOutput,
  // Platform detection utilities
  isWindows,
  isMacOS,
  platform: platform(),
  supportedPlatforms: ['win32', 'darwin']
};