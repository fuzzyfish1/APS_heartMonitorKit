"""
Simple live serial plotter.
Parses lines like:  signal: 512,slope: 23,bpm: 72
Usage: python3 plotter.py /dev/ttyUSB0
"""

import sys, serial, re
import matplotlib
#matplotlib.use("gtk3agg")
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque, OrderedDict

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 9600
WINDOW = 300

ser = serial.Serial(PORT, BAUD, timeout=0.05)
fig, ax = plt.subplots()
buffers = OrderedDict()
lines = {}

def update(frame):
    for _ in range(20):
        try:
            raw = ser.readline().decode(errors="ignore").strip()
            if not raw:
                break
            pairs = re.findall(r'([^:,]+):\s*([0-9.\-]+)', raw)
            for name, val in pairs:
                name = name.strip()
                if name not in buffers:
                    buffers[name] = deque([0]*WINDOW, maxlen=WINDOW)
                    lines[name], = ax.plot([], [], label=name)
                    ax.legend()
                buffers[name].append(float(val))
                lines[name].set_data(range(WINDOW), list(buffers[name]))
        except:
            continue
    ax.relim()
    ax.autoscale_view()

ani = animation.FuncAnimation(fig, update, interval=30, cache_frame_data=False)
plt.show()
ser.close()
