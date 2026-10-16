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

FCC_FILE = "/sys/bus/iio/devices/iio:device5/in_current_main_constant_charge_current_max_input"
USB_ONLINE = "/sys/class/power_supply/usb/online"
BATT_CAPACITY = "/sys/class/power_supply/qcom_battery/capacity"
BATT_CURRENT = "/sys/class/power_supply/qcom_battery/current_now"
BATT_VOLTAGE = "/sys/class/power_supply/qcom_battery/voltage_now"
TOTAL_CAP_UAH = 5105000

EV_SYN = 0
EV_KEY = 1
EV_ABS = 3

KEY_POWER = 116
KEY_WAKEUP = 143
BTN_TOUCH = 330

def get_voltage_soc(v_uv, current_ua):
    ocv_mv = (v_uv / 1000.0) - ((current_ua / 1000000.0) * 60.0)
    if ocv_mv >= 4350:
        return 100
    elif ocv_mv >= 4200:
        return int(85 + (ocv_mv - 4200) / 150.0 * 15)
    elif ocv_mv >= 4000:
        return int(60 + (ocv_mv - 4000) / 200.0 * 25)
    elif ocv_mv >= 3850:
        return int(40 + (ocv_mv - 3850) / 150.0 * 20)
    elif ocv_mv >= 3750:
        return int(20 + (ocv_mv - 3750) / 100.0 * 20)
    elif ocv_mv >= 3650:
        return int(10 + (ocv_mv - 3650) / 100.0 * 10)
    elif ocv_mv >= 3500:
        return int(3 + (ocv_mv - 3500) / 150.0 * 7)
    else:
        return 1


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
    last_chg_check = 0
    last_batt_time = time.time()
    estimated_soc = None

    def manage_battery(current_time):
        nonlocal last_chg_check, last_batt_time, estimated_soc
        if current_time - last_chg_check < 3.0:
            return
        dt = current_time - last_batt_time
        last_batt_time = current_time
        last_chg_check = current_time

        # 1. Maintain Fast Charge Current (2.5A) when charger is plugged in
        try:
            if os.path.exists(USB_ONLINE) and os.path.exists(FCC_FILE):
                with open(USB_ONLINE, "r") as f:
                    online = f.read().strip()
                if online == "1":
                    with open(FCC_FILE, "r") as f:
                        cur_fcc = int(f.read().strip())
                    if cur_fcc < 2000000:
                        with open(FCC_FILE, "w") as f:
                            f.write("2500000\n")
        except Exception:
            pass

        # 2. Track & update battery capacity
        try:
            if os.path.exists(BATT_VOLTAGE) and os.path.exists(BATT_CURRENT) and os.path.exists(BATT_CAPACITY):
                with open(BATT_VOLTAGE, "r") as f:
                    v_uv = int(f.read().strip())
                with open(BATT_CURRENT, "r") as f:
                    i_ua = int(f.read().strip())

                v_soc = get_voltage_soc(v_uv, i_ua)

                if estimated_soc is None:
                    estimated_soc = float(v_soc)
                else:
                    # In qcom_battery, positive current = charging, negative = discharging
                    delta_uah = i_ua * (dt / 3600.0)
                    delta_soc = (delta_uah / TOTAL_CAP_UAH) * 100.0
                    estimated_soc += delta_soc
                    # Smooth blend with voltage OCV curve to prevent drift
                    estimated_soc = estimated_soc * 0.98 + v_soc * 0.02
                    estimated_soc = max(1.0, min(100.0, estimated_soc))

                new_soc = int(round(estimated_soc))
                with open(BATT_CAPACITY, "r") as f:
                    cur_soc = int(f.read().strip())
                if new_soc != cur_soc:
                    with open(BATT_CAPACITY, "w") as f:
                        f.write(f"{new_soc}\n")
        except Exception:
            pass

    fds = [fd_pwr]
    if fd_touch is not None:
        fds.append(fd_touch)

    while running:
        now = time.time()
        manage_battery(now)
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
