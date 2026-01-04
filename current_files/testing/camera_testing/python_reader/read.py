import serial
import time
from serial.serialutil import SerialException

PORT = "COM9"
BAUD = 115200
OUT_FILE = "capture.jpg"

def capture_once():
    with serial.Serial(PORT, BAUD, timeout=15) as ser:
        time.sleep(2)
        ser.reset_input_buffer()

        input("Press ENTER to capture image...")
        ser.write(b"c")

        header = ""
        start = time.time()

        while time.time() - start < 10:
            line = ser.readline().decode(errors="ignore").strip()
            if not line:
                continue
            print("RX:", line)

            if line.startswith("JPG "):
                header = line
                break

            if line.startswith("ERR"):
                raise RuntimeError(f"ESP32 error: {line}")

        if not header:
            raise RuntimeError("No JPG header received")

        length = int(header.split()[1])
        print("Receiving", length, "bytes")

        data = ser.read(length)
        if len(data) != length:
            raise RuntimeError(f"Incomplete frame: got {len(data)} bytes")

        with open(OUT_FILE, "wb") as f:
            f.write(data)

        print("Saved:", OUT_FILE)

for attempt in range(1, 4):
    try:
        capture_once()
        break
    except (RuntimeError, SerialException, PermissionError) as e:
        print(f"Attempt {attempt} failed:", e)
        time.sleep(2)