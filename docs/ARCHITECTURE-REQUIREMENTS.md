# Architecture Requirements - CRITICAL

## ⚠️ MANDATORY ARCHITECTURE: arm64 ONLY

**DO NOT CHANGE TO x64/x86_64 - THIS WILL BREAK EVERYTHING**

### Why arm64 is Required

1. **Universal Syphon Framework**: We built a custom Universal Syphon framework from source that supports arm64
2. **Apple Silicon Native**: Modern macOS development requires native arm64 support
3. **Performance**: arm64 provides better performance on Apple Silicon
4. **Rust + Neon**: Our Rust implementation is specifically built for arm64

### Current Working Configuration

- ✅ **System**: arm64 macOS (Apple Silicon)
- ✅ **Syphon Framework**: Universal (x86_64 + arm64) - `/frameworks/Syphon.framework`
- ✅ **Rust Module**: Built for arm64 target
- ✅ **Node.js**: MUST run in arm64 mode (not Rosetta/x64)
- ✅ **Electron**: MUST use arm64 build

### Node.js Architecture Fix

**Current Issue**: Node.js is running in x64 mode (Rosetta) instead of arm64

**Solution**: 
```bash
# Check current Node.js architecture
node -p "process.arch"  # Should return "arm64", not "x64"

# If showing "x64", reinstall Node.js for arm64:
# - Download arm64 Node.js from nodejs.org
# - OR use nvm to install arm64 version
# - OR use Homebrew: brew install node
```

### Build Requirements

1. **Rust Target**: `aarch64-apple-darwin` (arm64)
2. **Syphon Framework**: Universal binary already built
3. **Native Module**: Must be arm64 compatible

### Testing Requirements

All tests must run on:
- ✅ **Node.js arm64**: Standalone Syphon functionality
- ✅ **Electron arm64**: Full integration testing

### NEVER DO THIS

- ❌ Don't build for x86_64/x64 to "fix" architecture mismatches
- ❌ Don't use Rosetta/x64 Node.js
- ❌ Don't rebuild Syphon framework for x64 only
- ❌ Don't use cmake-js/node-gyp (we use Rust + Neon)

### Architecture Verification Commands

```bash
# System architecture (should be arm64)
uname -m

# Node.js architecture (MUST be arm64)
node -p "process.arch"

# Native module architecture (should be arm64)
file build/Release/electron-video-share.node

# Syphon framework architecture (should show both x86_64 and arm64)
file frameworks/Syphon.framework/Syphon
```

### If Architecture Issues Occur

1. **First**: Check Node.js is running arm64, not x64
2. **Second**: Rebuild Rust module for correct target
3. **Never**: Change to x64 - fix the environment instead

---

**REMEMBER**: We have working arm64 Syphon integration. Keep it that way!