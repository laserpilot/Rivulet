# electron-video-share

Cross-platform video sharing for Electron applications using **Spout** (Windows) and **Syphon** (macOS).

## Features

- 🌐 **Cross-Platform**: Works on both Windows and macOS
- ⚡ **Hardware Accelerated**: Zero-copy video sharing using native frameworks
- 🎯 **Unified API**: Single interface that works on both platforms
- 📦 **Easy Integration**: Simple npm install and JavaScript API
- 🔧 **Modern**: Built for latest Electron versions with TypeScript support

## Installation

```bash
npm install electron-video-share
```

## Quick Start

```javascript
const { VideoOutput } = require('electron-video-share');
const { BrowserWindow } = require('electron');

// Create an offscreen Electron window
const win = new BrowserWindow({
  webPreferences: {
    offscreen: true,
    offscreenUseSharedTexture: true
  },
  show: false
});

// Create video output (automatically uses Spout on Windows, Syphon on macOS)
const output = new VideoOutput("My App Output");

// Share the window's rendered content
win.webContents.setFrameRate(60);
win.webContents.on("paint", (event, dirty, image, texture) => {
  if (texture) {
    // Hardware accelerated path (recommended)
    output.updateTexture(texture);
  } else {
    // Software fallback
    output.updateFrame(image.getBitmap(), image.getSize());
  }
});

win.loadURL('https://example.com');
```

## Platform Support

| Platform | Framework | Requirements |
|----------|-----------|--------------|
| Windows  | Spout     | Windows 10+ with DirectX 11 |
| macOS    | Syphon    | macOS 10.15+ with OpenGL/Metal |

## API Reference

### VideoOutput

Main class for cross-platform video sharing.

#### Constructor

```javascript
new VideoOutput(name, options)
```

- `name` (string): Human-readable output name
- `options` (object, optional): Platform-specific options

#### Methods

##### `updateFrame(bitmap, size)`
Update output with bitmap data.
- `bitmap` (Buffer): Frame bitmap data
- `size` (object): Frame dimensions `{width, height}`

##### `updateTexture(texture)`
Update output with texture data (hardware accelerated).
- `texture`: Platform-specific texture handle

##### `getName()`
Returns the output name.

##### `getPlatformInfo()`
Returns platform information:
```javascript
{
  platform: 'win32' | 'darwin',
  framework: 'Spout' | 'Syphon',
  version: '2.0.0'
}
```

##### `hasClients()`
Returns `true` if clients are connected.

##### `stop()`
Stop the output and cleanup resources.

### Platform Detection

```javascript
const { isWindows, isMacOS, platform } = require('electron-video-share');

console.log(`Running on: ${platform}`);
console.log(`Windows: ${isWindows}`);
console.log(`macOS: ${isMacOS}`);
```

## Building from Source

### Prerequisites

#### Windows
- Visual Studio 2022
- Node.js 16+
- CMake
- vcpkg (for Spout2)

#### macOS
- Xcode
- Node.js 16+
- CMake

### Build Steps

```bash
# Clone repository
git clone https://github.com/yourusername/electron-video-share.git
cd electron-video-share

# Install dependencies
npm install

# Build native module
npm run build

# Run tests
npm test
```

## Examples

### Basic Usage
```javascript
const { VideoOutput } = require('electron-video-share');
const output = new VideoOutput("My App");

// Use with Electron paint events
win.webContents.on("paint", (event, dirty, image, texture) => {
  output.updateTexture(texture);
});
```

### Platform-Specific Features
```javascript
const { VideoOutput } = require('electron-video-share');
const output = new VideoOutput("My App");

const info = output.getPlatformInfo();
if (info.framework === 'Spout') {
  // Windows-specific Spout features
  console.log('Using Spout on Windows');
} else if (info.framework === 'Syphon') {
  // macOS-specific Syphon features
  console.log('Using Syphon on macOS');
}
```

## Receiving Applications

### Windows (Spout)
- [Spout Receiver](https://github.com/leadedge/Spout2/releases)
- OBS Studio (with Spout2 plugin)
- Resolume Arena/Avenue
- TouchDesigner
- MadMapper

### macOS (Syphon)
- [Syphon Recorder](http://syphon.info/)
- OBS Studio (with Syphon plugin)
- Resolume Arena/Avenue
- TouchDesigner
- MadMapper
- VDMX

## Troubleshooting

### Common Issues

#### Module not found
```bash
# Rebuild native module
npm run rebuild
```

#### Windows: Spout not working
- Ensure DirectX 11 is available
- Check if antivirus is blocking the application
- Verify Visual Studio redistributables are installed

#### macOS: Syphon not working
- Ensure app has screen recording permissions
- Check if OpenGL/Metal is available
- Verify Xcode command line tools are installed

### Debug Mode

```javascript
const { VideoOutput } = require('electron-video-share');
const output = new VideoOutput("Debug Output", { debug: true });
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## Related Projects

- [Spout](https://github.com/leadedge/Spout2) - Windows video sharing framework
- [Syphon](https://github.com/Syphon/Syphon-Framework) - macOS video sharing framework
- [Electron](https://electronjs.org/) - Cross-platform desktop applications