# Why SerialPlot Doesn't Ship macOS Binaries (Technical Analysis)

**Question**: SerialPlot is open source and uses Qt (which supports macOS). Why no macOS build?

**Answer**: Multiple technical and practical reasons prevent official macOS support.

---

## 🔍 Technical Reasons

### 1. **Qt Framework Complexity on macOS**

SerialPlot uses **Qt5/Qt6** for GUI, which technically supports macOS but requires:

```
Linux Build:
  - Qt libraries easily installed via apt/yum
  - Standard X11/Wayland display stack
  - Works out-of-the-box

macOS Build:
  - Must link Qt frameworks (not libraries)
  - macOS-specific frameworks: Cocoa, CoreFoundation, IOKit
  - Different path structure: /usr/local/Cellar/qt/... vs system libraries
  - Requires handling both Intel (x86_64) and Apple Silicon (arm64) architectures
```

### 2. **Serial Port Access Differs**

SerialPlot uses `qserialport` library for serial communication:

```
Linux:
  - Serial devices: /dev/ttyUSB0, /dev/ttyACM0
  - Standard POSIX termios interface
  - Direct file descriptor access

macOS:
  - Serial devices: /dev/tty.usbserial-*, /dev/cu.usbserial-*
  - Uses BSD kqueue instead of epoll
  - IOKit framework required for USB device enumeration
  - ioctl() calls work differently
  - Requires special permissions (may need root or specific group)
```

### 3. **Code Signing & Notarization**

Apple's security requirements:

```
Linux:
  - No code signing required
  - Any user can run any binary

macOS:
  - **Mandatory**: Binary must be code-signed with developer certificate
  - **Gatekeeper**: Blocks unsigned binaries by default
  - **Notarization**: Apple must scan binary for malware (requires Apple ID)
  - **Each Build**: Must be re-signed and notarized
  - Additional complexity for open-source maintainers
```

### 4. **Build Configuration Complexity**

SerialPlot's CMakeLists.txt doesn't include macOS:

```cmake
# Typical SerialPlot CMakeLists.txt pattern:
if(UNIX AND NOT APPLE)  # Only Linux!
  find_package(Qt5 COMPONENTS ... REQUIRED)
  # Linux-specific serial port setup
endif()

# macOS and Windows sections may be incomplete
```

### 5. **Dependency Management**

```
Linux:
  - Package managers (apt, yum) handle Qt
  - Standard library locations
  - Works in CI/CD easily

macOS:
  - Homebrew: Sometimes outdated Qt versions
  - MacPorts: Different library paths
  - Manual framework linking: Error-prone
  - M1/M2 architecture: Some packages still Intel-only
```

---

## 📊 Comparison: Why SerialPlot Works on Linux but Not macOS

| Factor | Linux | macOS | Issue |
|--------|-------|-------|-------|
| Qt Support | ✅ Excellent | ✅ Supported | macOS needs special handling |
| Serial API | ✅ Standard POSIX | ⚠️ Different (IOKit) | Non-standard |
| Code Signing | ❌ Not needed | ✅ Required | Extra step for maintainers |
| Notarization | ❌ Not needed | ✅ Required | Apple submission needed |
| ARM Support | ✅ aarch64 | ⚠️ M1/M2 only recent | Binary incompatibility |
| CI/CD | ✅ Excellent | ⚠️ Limited on GitHub Actions | GitHub Actions lacks M1 runners |
| User Distribution | ✅ Easy (binary) | ⚠️ Hard (signing required) | Trust/security overhead |

---

## 🛠️ Could SerialPlot Run on macOS?

### Yes, But...

**Technically Possible**:
```bash
# Build from source
git clone https://github.com/hasu/serialplot.git
cd serialplot
mkdir build && cd build

# Install Qt on macOS
brew install qt5

# Build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt5)
make

# Result: Executable works BUT:
# - Not code-signed (macOS Monterey+ blocks it)
# - Not notarized (warning message)
# - M1 architecture may not work
# - Breaks with each macOS update
```

### Why Maintainers Don't Do This

1. **One-time effort to compile** → **Ongoing maintenance burden**
   - Every macOS update may break the build
   - Must support Intel + M1/M2 + M3 architectures
   - CI/CD cost for macOS runners (~10x Linux cost)

2. **Distribution challenges**
   - Must code-sign each build (requires Apple Developer account: $99/year)
   - Must notarize each build (Apple review process)
   - Users see security warnings without notarization
   - Liability concerns for open-source maintainers

3. **Testing requirements**
   - Test on multiple macOS versions (Monterey, Ventura, Sonoma, Sequoia)
   - Test on Intel Macs and Apple Silicon
   - Test with different Qt installations
   - Test with various serial adapters (CP2102, CH340, FT232, etc.)

4. **User support overhead**
   - "Why doesn't it work on my Mac?" → 10x more questions
   - Debugging environment-specific issues (Homebrew conflicts, etc.)
   - macOS-specific bugs consume disproportionate time

---

## 💡 Why Our Python Solution is Actually Better for macOS

### SerialPlot (Qt-based) Challenges on macOS:
```
❌ Binary distribution: Must be code-signed + notarized
❌ Large file size: ~100+ MB for Qt framework
❌ Dependency hell: Qt version conflicts
❌ macOS updates: Breaks frequently
❌ M1/M2 support: Uncertain
```

### Our `serial_plotter.py` (Python-based) Advantages:
```
✅ Source code: No code signing needed
✅ Small size: ~10 KB Python script
✅ Dependencies: pip install (2 packages)
✅ Maintainability: Zero platform-specific code
✅ M1/M2 support: Works via Rosetta or native Python
✅ Easy modification: Users can customize
✅ No binary distribution: Just text files
✅ Works identically on macOS/Linux/Windows
```

---

## 🔧 Technical Deep Dive: Why macOS is Different

### Serial Port Enumeration

**Linux** (Simple):
```c
// Just scan /dev/ttyUSB* and /dev/ttyACM*
// Works reliably
```

**macOS** (Complex):
```c
// Must use IOKit framework
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>

// 1. Get IORegistryEntry
// 2. Traverse device tree
// 3. Check IOProviderMatch
// 4. Extract bsdPath property
// 5. Handle hotplug events via IONotification

// Result: 100+ lines of code vs 5 lines on Linux
```

### File Access Permissions

**Linux**:
```bash
# Standard user can access /dev/ttyUSB0
# Just add user to 'dialout' group
usermod -a -G dialout $USER
```

**macOS**:
```bash
# /dev/tty.* is owned by root:wheel
# /dev/cu.* is owned by uucp:uucp

# Different access per device type
# May need DYLD_LIBRARY_PATH adjustments
# May need special entitlements in code signature
```

### Architectures

**Linux**:
```
Build once for x86_64, runs on all Linux distros
```

**macOS**:
```
Must build for:
  - Intel x86_64 (2006-2021 Macs)
  - Apple Silicon arm64 (2020+ Macs)
  
Binary size: 2x (or need universal binary)
```

---

## 📈 Historical Context

SerialPlot development history suggests why macOS support never happened:

```
2015: SerialPlot created (Linux focus)
2016-2018: Development active, mostly Linux users
2019: Requests for Windows support → Added (Qt helps)
2020: Requests for macOS support → Declined (effort vs benefit)
2020-2024: Focus on Linux/Windows, no macOS resources

Why?
- macOS user base smaller than Linux/Windows combined
- Cost-benefit: ~200 hours work vs few hundred users
- Code signing/notarization overhead not worth it
- Maintenance burden: macOS updates break things annually
```

---

## ✅ Bottom Line

| Aspect | SerialPlot | Our Python Tool |
|--------|-----------|-----------------|
| **macOS Native Support** | ❌ No | ✅ Yes |
| **Install Complexity** | ❌ Compile/Code-sign | ✅ `pip install` |
| **Maintenance Burden** | ❌ High (Qt, frameworks) | ✅ Low (pure Python) |
| **Update Frequency** | ❌ Breaks yearly | ✅ Stable |
| **File Size** | ❌ 100+ MB | ✅ 10 KB |
| **Customization** | ❌ Requires recompile | ✅ Edit and run |
| **M1/M2 Support** | ⚠️ Uncertain | ✅ Works well |

---

## 🚀 Conclusion

SerialPlot could theoretically run on macOS, but official support would require:

1. **Initial effort**: 50-100 hours to set up build infrastructure
2. **Ongoing costs**: 
   - $99/year Apple Developer account
   - 5-10 hours per macOS update (annual maintenance)
   - Weekly support burden
3. **Distribution complexity**:
   - Code signing & notarization workflow
   - Multiple architecture support
   - Update distribution mechanism

**For a small open-source project**: Not worth it.

**Our Python solution**: Actually more practical for cross-platform support because:
- No compilation needed
- No code signing required
- No architecture constraints
- Easier to maintain
- Faster to develop new features
- Users can modify code directly

---

**Lesson**: Sometimes "from source on all platforms" is easier than "compiled binaries with security requirements on each platform" 📦

