# Serial Plotter Guide - macOS/Linux/Windows

**Alternative to SerialPlot** - Works on all platforms including macOS

## What is serial_plotter.py?

A real-time force visualization tool that:
- ✅ Works on macOS, Linux, and Windows
- ✅ Plots all 4 channels simultaneously  
- ✅ Shows live statistics (min/max/average)
- ✅ Auto-scales plots
- ✅ No external dependencies beyond Python standard library + matplotlib

## Installation

### Requirements
```bash
# Python 3.6+ with pip
python3 --version  # Should be 3.6 or higher

# Install dependencies
pip3 install pyserial matplotlib
```

### Verify Installation
```bash
python3 serial_plotter.py --help
```

Expected output:
```
usage: serial_plotter.py [-h] [--port PORT] [--baud BAUD]

GRF Force Platform - Real-time Serial Plotter

optional arguments:
  -h, --help     show this help message and exit
  --port PORT    Serial port (default: /dev/ttyUSB0)
  --baud BAUD    Baud rate (default: 921600)
```

## Usage

### Method 1: Using Makefile (Recommended)
```bash
make plot                                    # Use default port (/dev/ttyUSB0)
make plot PORT=/dev/ttyACM0 BAUD=115200    # Custom port/baud
```

### Method 2: Direct Command
```bash
cd /Users/zhaojia/github/ads1261

# macOS
python3 serial_plotter.py --port /dev/ttyUSB0 --baud 921600

# Linux
python3 serial_plotter.py --port /dev/ttyUSB0 --baud 921600

# Windows (PowerShell)
python serial_plotter.py --port COM3 --baud 921600
```

## GUI Features

### Plot Display
- **4 subplots** - One for each force channel
- **Color-coded** - Ch1: Red, Ch2: Blue, Ch3: Green, Ch4: Purple
- **Auto-scaling** - Y-axis automatically adjusts to data range
- **Real-time updates** - Plots refresh every 200ms

### Statistics Panel
Shows for each channel:
- **Current average**: Mean force over all collected samples
- **Minimum**: Lowest recorded force
- **Maximum**: Highest recorded force

### Controls
- **Clear Button**: Reset all data and restart plotting
- **Window Close**: Properly closes serial connection

## Workflow Example

### 1. Start with Hardware Connected
```bash
make flash-monitor PORT=/dev/ttyUSB0
# Wait for "Ground Reaction Force Measurement Starting..."
# Then press Ctrl+C to exit monitor
```

### 2. Launch Plotter in New Terminal
```bash
make plot PORT=/dev/ttyUSB0
# Should see 4 subplots with real-time data
```

### 3. Apply Test Loads
- Observe force values updating in real-time
- Check that all 4 channels respond appropriately
- Verify measurements are stable (±1-2 N variation)

### 4. Analyze Results
- Look for outliers or unstable channels
- Check that force increases monotonically with load
- Verify channels track together (similar response)

### 5. Export Data for Analysis
Option A: Pipe to file:
```bash
python3 serial_plotter.py --port /dev/ttyUSB0 2>&1 | tee output.txt
```

Option B: Modify code for CSV output:
```bash
# Edit main.c: Change OUTPUT_FORMAT from OUTPUT_FORMAT_HUMAN to OUTPUT_FORMAT_CSV
# Rebuild and flash: make flash
# Plotter will still work, or use terminal redirect:
idf.py -p /dev/ttyUSB0 monitor | tee data.csv
```

## Troubleshooting

### "Failed to connect" Error
```bash
# Check port name
ls /dev/tty.* /dev/cu.*     # macOS
ls /dev/ttyUSB* /dev/ttyACM*  # Linux
mode COM3                     # Windows

# Use correct port:
make plot PORT=/dev/ttyUSB0
```

### "ModuleNotFoundError: No module named 'serial'"
```bash
pip3 install pyserial
# or
python3 -m pip install pyserial
```

### "No module named 'matplotlib'"
```bash
pip3 install matplotlib
# or
python3 -m pip install matplotlib
```

### Plots Not Updating
1. Check serial port is correct
2. Ensure ESP32-C6 is running (should see "Force readings" in monitor)
3. Verify baud rate matches (921600 by default)
4. Try closing and reopening plotter

### Wrong Port on macOS?
```bash
# List all serial ports
ls -la /dev/tty.* /dev/cu.*

# Usually looks like:
# /dev/tty.usbserial-XXXXXXXX  (for USB serial)
# /dev/tty.SLAB_USBtoUART      (for CP2102)

# Use the one that appears when board is plugged in:
make plot PORT=/dev/tty.usbserial-12345678
```

## Output Formats

The plotter automatically parses both formats:

### Format 1: Human-Readable (Default)
```
[Frame 100] Force readings (N):
  Ch1: 25.34 N (raw=7f5000, norm=0.0312)
  Ch2: 24.99 N (raw=7f4f80, norm=0.0311)
  Ch3: 25.02 N (raw=7f5010, norm=0.0312)
  Ch4: 24.98 N (raw=7f4f70, norm=0.0311)
  Total: 100.33 N
```

### Format 2: CSV (for data logging)
Edit `main.c`:
```c
#define OUTPUT_FORMAT  OUTPUT_FORMAT_CSV  // Change from OUTPUT_FORMAT_HUMAN
```

Output:
```
frame_number,timestamp_us,ch1_N,ch2_N,ch3_N,ch4_N,total_N
100,1234567890,25.34,24.99,25.02,24.98,100.33
101,1234567900,25.35,25.00,25.03,24.99,100.37
```

## Performance Notes

- **Refresh Rate**: 200ms per update (5 Hz plot updates)
- **Buffer Size**: 500 samples per channel (adjustable in code)
- **Memory**: ~50 MB Python process
- **CPU**: <5% on modern systems

## Advanced Usage

### Save Plot to Image
```python
# Modify serial_plotter.py to save on key press
# Add to PlotterGUI class:
self.fig.savefig('force_plot.png', dpi=150)
```

### Export Data from Running Plotter
```bash
# Redirect stdout to file while running
make plot > force_data.log 2>&1 &
# Stop with: Ctrl+C
```

### Real-time Analysis (Advanced)
```bash
# Pipe to awk for on-the-fly statistics
python3 serial_plotter.py 2>&1 | grep "Ch1:" | awk '{print $NF}' | tail -20 | awk '{sum+=$1; count++} END {print "Avg:", sum/count}'
```

## Comparison: SerialPlot vs serial_plotter.py

| Feature | SerialPlot | serial_plotter.py |
|---------|-----------|------------------|
| macOS Support | ❌ No (Windows/Linux only) | ✅ Yes |
| Linux Support | ✅ Yes | ✅ Yes |
| Windows Support | ✅ Yes | ✅ Yes |
| Installation | Binary download | `pip3 install` |
| Source Code | Closed | Open (python3 script) |
| Customization | Limited | Full control |
| Performance | Native compiled | Python (adequate) |
| Dependencies | None | matplotlib, pyserial |

## Next Steps

1. ✅ Install dependencies: `pip3 install pyserial matplotlib`
2. ✅ Flash ESP32-C6: `make flash-monitor`
3. ✅ Start plotter: `make plot`
4. ✅ Apply test loads and observe plots
5. ✅ Verify all 4 channels respond correctly
6. ✅ Use for continuous GRF measurement validation

---

**Questions?** Check [INDEX.md](INDEX.md) for full project documentation.
