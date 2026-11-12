import argparse
import ctypes
import glob
import os
import time
from ctypes import wintypes
from datetime import datetime

import serial


class FileTime(ctypes.Structure):
    _fields_ = [
        ("low", wintypes.DWORD),
        ("high", wintypes.DWORD),
    ]


class MemoryStatusEx(ctypes.Structure):
    _fields_ = [
        ("length", wintypes.DWORD),
        ("memory_load", wintypes.DWORD),
        ("total_physical", ctypes.c_ulonglong),
        ("available_physical", ctypes.c_ulonglong),
        ("total_page_file", ctypes.c_ulonglong),
        ("available_page_file", ctypes.c_ulonglong),
        ("total_virtual", ctypes.c_ulonglong),
        ("available_virtual", ctypes.c_ulonglong),
        ("available_extended_virtual", ctypes.c_ulonglong),
    ]


def filetime_value(value):
    return (value.high << 32) | value.low


def system_times():
    idle = FileTime()
    kernel = FileTime()
    user = FileTime()
    if not ctypes.windll.kernel32.GetSystemTimes(
        ctypes.byref(idle), ctypes.byref(kernel), ctypes.byref(user)
    ):
        raise ctypes.WinError()
    return filetime_value(idle), filetime_value(kernel), filetime_value(user)


def cpu_percent(previous, current):
    idle_delta = current[0] - previous[0]
    total_delta = (current[1] - previous[1]) + (current[2] - previous[2])
    if total_delta <= 0:
        return 0
    return max(0, min(100, round(100 * (total_delta - idle_delta) / total_delta)))


def memory_percent():
    status = MemoryStatusEx()
    status.length = ctypes.sizeof(status)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        raise ctypes.WinError()
    return int(status.memory_load)


def desktop_frame_rgb565():
    try:
        from PIL import Image, ImageGrab
    except ImportError as error:
        raise RuntimeError(
            "Desktop preview requires Pillow; install it with "
            "'py -m pip install -r host\\requirements.txt'."
        ) from error

    screenshot = ImageGrab.grab().convert("RGB")
    screenshot.thumbnail((120, 120), Image.LANCZOS)
    frame = Image.new("RGB", (120, 120), (0, 0, 0))
    frame.paste(
        screenshot,
        ((120 - screenshot.width) // 2, (120 - screenshot.height) // 2),
    )

    data = bytearray()
    for red, green, blue in frame.getdata():
        color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        data.append((color >> 8) & 0xFF)
        data.append(color & 0xFF)
    return data


def send_frame(device, data):
    if len(data) != 120 * 120 * 2:
        raise ValueError("A display frame must contain exactly 28,800 bytes")
    device.write(b"IMG\n")
    for offset in range(0, len(data), 240):
        device.write(data[offset : offset + 240])
    device.flush()


def stream_desktop(device):
    print("Streaming desktop preview; press Ctrl+C to stop.")
    while True:
        send_frame(device, desktop_frame_rgb565())


def send_images(device, image_dir, interval):
    images = sorted(glob.glob(os.path.join(image_dir, "*.rgb565")))
    if not images:
        raise ValueError("No .rgb565 images found in {}".format(image_dir))

    expected_size = 120 * 120 * 2
    while True:
        for path in images:
            with open(path, "rb") as image_file:
                data = image_file.read()
            if len(data) != expected_size:
                raise ValueError(
                    "{} has {} bytes; expected {}".format(
                        path, len(data), expected_size
                    )
                )

            print("Showing {}".format(os.path.basename(path)))
            send_frame(device, data)
            time.sleep(interval)


def main():
    parser = argparse.ArgumentParser(
        description="Send PC status, cycle pictures, or preview the desktop on the STM32 display."
    )
    parser.add_argument("port", help="CH340 serial port, for example COM5")
    parser.add_argument(
        "--text", default="Hello from Windows", help="ASCII note shown on the display"
    )
    display_mode = parser.add_mutually_exclusive_group()
    display_mode.add_argument(
        "--images",
        help="Directory containing 120x120 RGB565 .rgb565 images to cycle",
    )
    display_mode.add_argument(
        "--screen",
        action="store_true",
        help="Continuously preview the Windows desktop at low frame rate",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=3.0,
        help="Seconds to show each image after transfer (default: 3)",
    )
    args = parser.parse_args()

    if os.name != "nt":
        parser.error("This dashboard monitor currently supports Windows only.")

    if args.interval < 0:
        parser.error("--interval must be zero or greater")

    if args.images or args.screen:
        with serial.Serial(args.port, 115200, timeout=1) as device:
            if args.screen:
                stream_desktop(device)
            else:
                send_images(device, args.images, args.interval)
        return

    note = args.text.encode("ascii", errors="replace").decode("ascii")
    note = note.replace(",", " ").replace("\r", " ").replace("\n", " ")[:24]

    previous = system_times()
    with serial.Serial(args.port, 115200, timeout=1) as device:
        while True:
            time.sleep(1)
            current = system_times()
            cpu = cpu_percent(previous, current)
            previous = current
            memory = memory_percent()
            clock = datetime.now().strftime("%H:%M:%S")
            packet = "{},{},{},{}\n".format(cpu, memory, clock, note)
            device.write(packet.encode("ascii"))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
