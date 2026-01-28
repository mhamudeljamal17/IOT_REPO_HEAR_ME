import serial
import struct
import wave
import os
import time

SERIAL_PORT = "COM6"      # CHANGE THIS
BAUDRATE = 115200
WAV_DIR = "./wav_files"

def send_wav(ser, wav_path):
    with wave.open(wav_path, 'rb') as wf:
        assert wf.getnchannels() == 1
        assert wf.getsampwidth() == 2
        assert wf.getframerate() == 16000

        frames = wf.readframes(wf.getnframes())

    print(f"[PY] Sending {wav_path} ({len(frames)} bytes)")

    ser.write(b"START\n")
    ser.write(struct.pack("<I", len(frames)))
    ser.write(frames)
    ser.write(b"\nEND\n")
    ser.flush()

if __name__ == "__main__":
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
    time.sleep(2)

    for file in sorted(os.listdir(WAV_DIR)):
        if not file.endswith(".wav"):
            continue

        send_wav(ser, os.path.join(WAV_DIR, file))
        time.sleep(3)

    ser.close()
