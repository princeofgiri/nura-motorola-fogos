#!/usr/bin/env python3
"""
Motorola Moto G34 5G (fogos) Power & Display Wakeup Daemon
Handles:
1. Power button press: Instant toggle between Sleep (backlight 0 + lock) and Wakeup.
2. Double-tap to wake: Wakes up screen on double-tap when asleep.
3. Auto-sleep idle timeout: Automatically sleeps and locks screen after inactivity.
"""

import os
import sys
import time
import glob
import fcntl
import signal
import struct
import select
import subprocess

BRIGHTNESS_FILE = "/sys/class/backlight/panel0-backlight/brightness"
GESTURE_FILE = "/sys/devices/virtual/touchscreen/primary/gesture"
IDLE_TIMEOUT_SEC = 120  # 2 minutes auto-sleep
EVENT_SIZE = 24
EVENT_FORMAT = "qqHHi"
EVIOCGRAB = 0x40044590

EV_SYN = 0
EV_KEY = 1
EV_ABS = 3

KEY_POWER = 116
KEY_WAKEUP = 143
BTN_TOUCH = 330

def find_input_device(name_substring):
    try:
        with open("/proc/bus/input/devices", "r") as f:
            content = f.read()
        blocks = content.split("\n\n")
        for b in blocks:
            if name_substring.lower() in b.lower():
                for line in b.split("\n"):
                    if line.startswith("H: Handlers="):
                        for part in line.split():
                            if part.startswith("event"):
                                dev = f"/dev/input/{part}"
                                if os.path.exists(dev):
                                    return dev
    except Exception as e:
        sys.stderr.write(f"Error parsing input devices: {e}\n")
    return None

def get_brightness():
    try:
        with open(BRIGHTNESS_FILE, "r") as f:
            return int(f.read().strip())
    except Exception:
        return 745

def set_brightness(val):
    try:
        with open(BRIGHTNESS_FILE, "w") as f:
            f.write(f"{val}\n")
    except Exception as e:
        sys.stderr.write(f"Error writing brightness: {e}\n")

def enable_dt2w():
    try:
        if os.path.exists(GESTURE_FILE):
            with open(GESTURE_FILE, "w") as f:
                f.write("1\n")
    except Exception as e:
        sys.stderr.write(f"Error enabling DT2W: {e}\n")

def lock_session():
    try:
        subprocess.run(["loginctl", "lock-session"], capture_output=True, timeout=2)
    except Exception:
        pass

def main():
    enable_dt2w()

    pwr_dev = find_input_device("qpnp_pon") or "/dev/input/event0"
    touch_dev = find_input_device("ILITEK_TDDI") or "/dev/input/event4"

    sys.stderr.write(f"fogos-powerd: Power device: {pwr_dev}, Touch device: {touch_dev}\n")

    try:
        fd_pwr = os.open(pwr_dev, os.O_RDONLY | os.O_NONBLOCK)
        fcntl.ioctl(fd_pwr, EVIOCGRAB, 1)
    except Exception as e:
        sys.stderr.write(f"fogos-powerd: Failed to grab power button: {e}\n")
        sys.exit(1)

    try:
        fd_touch = os.open(touch_dev, os.O_RDONLY | os.O_NONBLOCK)
    except Exception as e:
        sys.stderr.write(f"fogos-powerd: Failed to open touch device: {e}\n")
        fd_touch = None

    running = True
    def handle_sig(sig, frame):
        nonlocal running
        running = False
    signal.signal(signal.SIGTERM, handle_sig)
    signal.signal(signal.SIGINT, handle_sig)

    saved_brightness = max(100, get_brightness())
    last_user_activity = time.time()
    last_tap_time = 0

    fds = [fd_pwr]
    if fd_touch is not None:
        fds.append(fd_touch)

    while running:
        now = time.time()
        cur_bl = get_brightness()

        # Check idle timeout (only when screen is ON)
        if cur_bl > 0 and IDLE_TIMEOUT_SEC > 0:
            if now - last_user_activity > IDLE_TIMEOUT_SEC:
                saved_brightness = max(100, cur_bl)
                set_brightness(0)
                lock_session()
                last_user_activity = now
                continue

        r, _, _ = select.select(fds, [], [], 1.0)
        now = time.time()

        for fd in r:
            try:
                buf = os.read(fd, EVENT_SIZE * 16)
            except Exception:
                continue

            for i in range(0, len(buf), EVENT_SIZE):
                chunk = buf[i:i+EVENT_SIZE]
                if len(chunk) < EVENT_SIZE:
                    continue
                sec, usec, etype, code, val = struct.unpack(EVENT_FORMAT, chunk)

                # Reset idle timer on any input
                last_user_activity = now

                # 1. Power button event
                if fd == fd_pwr and etype == EV_KEY and code == KEY_POWER and val == 1:
                    cur_bl = get_brightness()
                    if cur_bl > 0:
                        # Screen is ON -> Sleep
                        saved_brightness = max(100, cur_bl)
                        set_brightness(0)
                        lock_session()
                    else:
                        # Screen is OFF -> Wakeup
                        set_brightness(saved_brightness)

                # 2. Touch event
                elif fd == fd_touch:
                    cur_bl = get_brightness()
                    if cur_bl == 0:
                        # Screen is sleeping -> check for Wakeup gesture
                        is_wake_key = (etype == EV_KEY and code in (KEY_POWER, KEY_WAKEUP) and val == 1)
                        is_double_tap = (etype == EV_KEY and code == BTN_TOUCH and val == 1 and (now - last_tap_time < 0.45))
                        
                        if is_wake_key or is_double_tap:
                            set_brightness(saved_brightness)
                            last_user_activity = now

                        if etype == EV_KEY and code == BTN_TOUCH and val == 1:
                            last_tap_time = now

    # Cleanup on exit
    try:
        fcntl.ioctl(fd_pwr, EVIOCGRAB, 0)
    except Exception:
        pass
    os.close(fd_pwr)
    if fd_touch is not None:
        os.close(fd_touch)

if __name__ == "__main__":
    main()
