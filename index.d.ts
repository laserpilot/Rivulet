export interface FrameSize {
  width: number;
  height: number;
}

export interface VideoOutputOptions {
  [key: string]: any;
}

export interface PlatformInfo {
  platform: string;
  framework: 'Spout' | 'Syphon';
  version: string;
}

/**
 * Cross-platform video output class
 * Uses Spout on Windows and Syphon on macOS
 */
export declare class VideoOutput {
  constructor(name: string, options?: VideoOutputOptions);
  
  /**
   * Update with frame data (bitmap)
   */
  updateFrame(bitmap: Buffer, size: FrameSize): void;
  
  /**
   * Update with texture data (when offscreenUseSharedTexture is true)
   */
  updateTexture(texture: any): void;
  
  /**
   * Get output name
   */
  getName(): string;
  
  /**
   * Get platform information
   */
  getPlatformInfo(): PlatformInfo;
  
  /**
   * Check if clients are connected
   */
  hasClients(): boolean;
  
  /**
   * Stop the output
   */
  stop(): void;
}

// Legacy compatibility
export declare class SpoutOutput extends VideoOutput {}
export declare class SyphonOutput extends VideoOutput {}

// Platform detection
export declare const isWindows: boolean;
export declare const isMacOS: boolean;
export declare const platform: string;
export declare const supportedPlatforms: string[];

// Default export
export { VideoOutput as default };