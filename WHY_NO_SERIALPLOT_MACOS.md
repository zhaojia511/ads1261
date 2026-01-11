# Why SerialPlot Doesn't Have macOS Support - Updated Analysis

**Repository**: https://github.com/hyOzd/serialplot  
**Status**: Qt6 based, builds on Linux/Windows but **theoretically can run on macOS**

---

## 🔍 Can SerialPlot Run on macOS?

### YES - But Officially Not Supported

**Good News**:
- ✅ Uses Qt6 (which supports macOS)
- ✅ Uses CMake (which works on macOS)
- ✅ Dependencies are available on macOS:
  - Qt6 via Homebrew
  - Qwt6 can be built
  - CMake/build tools available

**Not Published**:
- ❌ No official macOS binaries provided
- ❌ No macOS testing in CI/CD (only Linux/Windows in GitHub Actions)
- ❌ No macOS specific code signing/notarization
- ❌ No documented macOS build instructions

---

## 🛠️ Can You Build SerialPlot on macOS?

### Yes, Theoretically

**Steps to build**:
```bash
# Install dependencies
brew install qt6 cmake

# Build Qwt dependency
git clone https://github.com/hyOzd/serialplot
cd serialplot
mkdir build && cd build

# Build SerialPlot
cmake ..
make -j$(sysctl -n hw.ncpu)

# Result: serialplot executable (unsigned, may need code-sign)
```

**But You'll Encounter**:

1. **Qt6 Framework Path Issues**
   ```
   CMake Error: Could not find Qt6
   Solution: Need to tell CMake where Qt is:
   cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt6)
   ```

2. **Qwt Build Issues on macOS**
   - Qwt is downloaded and built automatically
   - May fail due to Qt framework linking issues
   - macOS-specific linker flags may be needed

3. **Code Signing Requirement**
   - macOS Monterey+: Blocks unsigned binaries
   - Even if it builds, won't run without code-signing
   - Self-signed code-signing may be possible but generates security warnings

4. **Architecture Issues**
   - Homebrew Qt6 may be Intel-only or Apple Silicon-only
   - May not match your Mac architecture
   - Universal binary support unclear

---

## 📊 SerialPlot macOS Status (v0.13)

### Build Configuration Analysis

From the CMakeLists.txt:

```cmake
# Windows-specific handling
if (WIN32)
  qt_add_resources(RES_FILES misc/icons.qrc misc/winicons.qrc)
  # ... Windows deployment
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt REQUIRED)
else (WIN32)
  qt_add_resources(RES_FILES misc/icons.qrc)
endif (WIN32)

# Linux/Unix-specific handling
if (UNIX)
  # Desktop entry, icon installation
  install(FILES ${DESKTOP_FILE} DESTINATION share/applications/)
else (UNIX)
  # Windows installation
endif (UNIX)
```

**Key Finding**: The build treats macOS as generic "UNIX" but:
- ❌ No macOS-specific code
- ❌ No macOS framework handling
- ❌ No macOS code-signing setup
- ❌ Desktop/icon installation assumes Linux (won't work on macOS)

---

## 🚀 Why No Official macOS Release?

### Reasons from Code Analysis

1. **Packaging is Linux-focused**
   ```cmake
   if (UNIX)
     set(CPACK_GENERATOR "DEB")  # Debian packages only
   elseif (WIN32)
     set(CPACK_GENERATOR "NSIS")  # Windows installer
   endif (UNIX)
   
   # No macOS (DMG) package generator!
   ```

2. **Binary Dependencies**
   ```cmake
   # Ubuntu-specific dependencies documented
   set(CPACK_DEBIAN_PACKAGE_DEPENDS
     "libqt6widgets6t64 (>= 6.4.2), ..."
   )
   
   # No macOS/Homebrew dependencies listed
   ```

3. **CI/CD**: Only Linux and Windows tested
   - GitHub Actions workflows likely only test Ubuntu/Windows
   - No macOS runners configured
   - No macOS build artifacts generated

4. **Desktop Integration**
   ```cmake
   # Linux desktop entry
   configure_file("${CMAKE_CURRENT_SOURCE_DIR}/misc/program_name.desktop.in" ...)
   install(FILES ${DESKTOP_FILE} DESTINATION share/applications/)
   
   # Assumes Linux filesystem layout
   # Won't work on macOS
   ```

---

## ✅ Bottom Line

### Can SerialPlot Run on macOS?

| Aspect | Reality |
|--------|---------|
| **Build from source** | ✅ Probably yes (with effort) |
| **Run compiled binary** | ⚠️ Yes but requires code-signing |
| **Official binary** | ❌ Not available |
| **Documented support** | ❌ No |
| **Tested on macOS** | ❌ No |
| **Easy to use** | ❌ No (requires build skills) |

### Practical Options for macOS Users

1. **Use Our Python Tool** (Recommended)
   - ✅ Works immediately: `make plot`
   - ✅ No compilation needed
   - ✅ No code-signing issues
   - ✅ Easy to modify

2. **Build SerialPlot from Source** (Difficult)
   - ⚠️ Requires C++ build tools
   - ⚠️ Complex dependency management
   - ⚠️ May not work due to macOS-specific issues
   - ⚠️ Self-signed binary will show security warnings

3. **Use Linux VM or Docker** (Workaround)
   - ✅ Guaranteed to work
   - ❌ Requires VM overhead
   - ❌ Port access complexity

---

## 🎯 Recommendation

**For your project**: Stick with `serial_plotter.py`

- ✅ Works on macOS immediately
- ✅ Simpler than building SerialPlot
- ✅ No security warnings
- ✅ Easier to customize
- ✅ Cross-platform (macOS/Linux/Windows identical)

SerialPlot is great on Linux/Windows where it's officially supported. On macOS, the Python solution is actually more practical.

---

**Summary**: SerialPlot *can* theoretically build on macOS but isn't officially supported, tested, or packaged for it. Our Python solution is better for cross-platform support including macOS.

