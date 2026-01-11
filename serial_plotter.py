#!/usr/bin/env python3
"""
GRF Force Platform - Real-time Serial Plotter for macOS/Linux/Windows

Reads force measurements from ESP32-C6 serial output and displays
real-time plots of all 4 channels using matplotlib.

Usage:
    python3 serial_plotter.py --port /dev/ttyUSB0 --baud 921600
"""

import sys
import argparse
import serial
import threading
import re
from collections import deque
from datetime import datetime

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import tkinter as tk
from tkinter import ttk, messagebox


class SerialPlotter:
    def __init__(self, port, baud=921600, max_points=500):
        """Initialize serial plotter."""
        self.port = port
        self.baud = baud
        self.max_points = max_points
        
        # Data storage for each channel
        self.data = {i: deque(maxlen=max_points) for i in range(1, 5)}
        self.timestamps = deque(maxlen=max_points)
        self.time_counter = 0
        
        # Serial connection
        self.ser = None
        self.reading = False
        self.read_thread = None
        
        # Statistics
        self.stats = {i: {'min': 0, 'max': 0, 'avg': 0} for i in range(1, 5)}
        
    def connect(self):
        """Connect to serial port."""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            print(f"✓ Connected to {self.port} at {self.baud} baud")
            return True
        except serial.SerialException as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from serial port."""
        self.reading = False
        if self.read_thread:
            self.read_thread.join(timeout=2)
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✓ Disconnected")
    
    def read_serial_data(self):
        """Read and parse serial data in background thread."""
        # Pattern to match: "Ch1: X.XX N (raw=XXXXXX, norm=X.XXXXXX)"
        pattern = r'Ch(\d):\s+([-\d.]+)\s+N\s+\(raw=([0-9a-f]+),\s+norm=([-\d.]+)\)'
        
        self.reading = True
        while self.reading:
            try:
                if self.ser and self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    # Look for channel data
                    matches = re.findall(pattern, line)
                    
                    if matches and len(matches) == 4:
                        # Got a complete frame with all 4 channels
                        for match in matches:
                            ch_num = int(match[0])
                            force_n = float(match[1])
                            raw_hex = match[2]
                            normalized = float(match[3])
                            
                            self.data[ch_num].append(force_n)
                        
                        self.timestamps.append(self.time_counter)
                        self.time_counter += 1
                        self.update_stats()
                        
            except (ValueError, UnicodeDecodeError):
                pass
            except Exception as e:
                print(f"Error reading serial: {e}")
    
    def start_reading(self):
        """Start background thread for serial reading."""
        if not self.reading:
            self.read_thread = threading.Thread(target=self.read_serial_data, daemon=True)
            self.read_thread.start()
            print("✓ Serial reading started")
    
    def update_stats(self):
        """Update statistics for each channel."""
        for ch in range(1, 5):
            if self.data[ch]:
                data_list = list(self.data[ch])
                self.stats[ch]['min'] = min(data_list)
                self.stats[ch]['max'] = max(data_list)
                self.stats[ch]['avg'] = sum(data_list) / len(data_list)
    
    def get_data(self):
        """Return current data for plotting."""
        return (list(self.timestamps), 
                {ch: list(self.data[ch]) for ch in range(1, 5)})


class PlotterGUI:
    """GUI for real-time force plotting."""
    
    def __init__(self, root, port, baud=921600):
        self.root = root
        self.root.title("GRF Force Platform - Real-time Plotter")
        self.root.geometry("1200x700")
        
        self.plotter = SerialPlotter(port, baud)
        
        # GUI elements
        self.setup_controls()
        self.setup_plot()
        
        if self.plotter.connect():
            self.plotter.start_reading()
            self.start_animation()
        else:
            messagebox.showerror("Connection Error", f"Failed to connect to {port}")
            self.root.quit()
    
    def setup_controls(self):
        """Setup control panel."""
        control_frame = ttk.Frame(self.root)
        control_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        
        # Connection info
        ttk.Label(control_frame, text=f"Port: {self.plotter.port} | Baud: {self.plotter.baud}").pack(side=tk.LEFT, padx=5)
        
        # Channel labels with colors
        colors = ['red', 'blue', 'green', 'purple']
        for i in range(1, 5):
            color_box = tk.Label(control_frame, text="■", font=("Arial", 16), fg=colors[i-1])
            color_box.pack(side=tk.LEFT, padx=2)
            ttk.Label(control_frame, text=f"Ch{i}").pack(side=tk.LEFT, padx=1)
        
        # Clear button
        ttk.Button(control_frame, text="Clear", command=self.clear_data).pack(side=tk.RIGHT, padx=5)
        
        # Stats frame
        self.stats_frame = ttk.Frame(self.root)
        self.stats_frame.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)
        self.stats_labels = {}
        for i in range(1, 5):
            label = ttk.Label(self.stats_frame, text=f"Ch{i}: -- N")
            label.pack(side=tk.LEFT, padx=10)
            self.stats_labels[i] = label
    
    def setup_plot(self):
        """Setup matplotlib plot."""
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 6))
        self.fig.suptitle("GRF Force Measurement - Real-time Display", fontsize=14)
        self.fig.tight_layout(rect=[0, 0.03, 1, 0.95])
        
        self.lines = {}
        colors = ['red', 'blue', 'green', 'purple']
        
        # Create subplots for each channel
        for i, ax in enumerate(self.axes.flat, 1):
            ax.set_title(f"Channel {i}")
            ax.set_xlabel("Sample #")
            ax.set_ylabel("Force (N)")
            ax.grid(True, alpha=0.3)
            ax.set_ylim([-50, 50])  # Default range, will auto-scale
            
            line, = ax.plot([], [], color=colors[i-1], linewidth=2, marker='o', markersize=2)
            self.lines[i] = line
        
        # Embed in tkinter
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.root)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
    
    def update_plot(self):
        """Update plot with new data."""
        timestamps, data = self.plotter.get_data()
        
        if not timestamps:
            return
        
        # Update each channel subplot
        for i, ax in enumerate(self.axes.flat, 1):
            if data[i]:
                self.lines[i].set_data(timestamps, data[i])
                
                # Auto-scale Y axis
                if len(data[i]) > 0:
                    data_min = min(data[i])
                    data_max = max(data[i])
                    margin = (data_max - data_min) * 0.1 if (data_max - data_min) > 0 else 5
                    ax.set_ylim([data_min - margin, data_max + margin])
                
                # Auto-scale X axis
                ax.set_xlim([0, max(timestamps) + 1] if timestamps else [0, 10])
        
        # Update statistics labels
        for i in range(1, 5):
            stats = self.plotter.stats[i]
            if stats['avg'] != 0:
                label_text = f"Ch{i}: {stats['avg']:.2f} N (min: {stats['min']:.2f}, max: {stats['max']:.2f})"
            else:
                label_text = f"Ch{i}: -- N"
            self.stats_labels[i].config(text=label_text)
        
        self.canvas.draw_idle()
    
    def start_animation(self):
        """Start animation loop."""
        self.anim = animation.FuncAnimation(
            self.fig, 
            lambda f: self.update_plot(), 
            interval=200,  # Update every 200ms
            blit=False
        )
    
    def clear_data(self):
        """Clear all data."""
        for ch in range(1, 5):
            self.plotter.data[ch].clear()
        self.plotter.timestamps.clear()
        self.plotter.time_counter = 0
        print("✓ Data cleared")
    
    def on_closing(self):
        """Handle window close."""
        self.plotter.disconnect()
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(
        description="GRF Force Platform - Real-time Serial Plotter"
    )
    parser.add_argument(
        '--port', 
        default='/dev/ttyUSB0',
        help='Serial port (default: /dev/ttyUSB0)'
    )
    parser.add_argument(
        '--baud',
        type=int,
        default=921600,
        help='Baud rate (default: 921600)'
    )
    
    args = parser.parse_args()
    
    print("\n" + "="*60)
    print("GRF Force Platform - Real-time Serial Plotter")
    print("="*60)
    print(f"Port: {args.port}")
    print(f"Baud: {args.baud}")
    print("\nPress Ctrl+C to exit")
    print("="*60 + "\n")
    
    root = tk.Tk()
    gui = PlotterGUI(root, args.port, args.baud)
    root.protocol("WM_DELETE_WINDOW", gui.on_closing)
    root.mainloop()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n✓ Exiting...")
    except Exception as e:
        print(f"\n✗ Error: {e}")
        sys.exit(1)
