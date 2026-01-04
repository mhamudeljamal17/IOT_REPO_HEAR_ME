# import serial
# import numpy as np
# import soundfile as sf
# import resampy
# import time
# import glob
# import os
#
# SERIAL_PORT = "COM9"      # Update if needed
# BAUD_RATE = 115200
# NUM_SAMPLES = 8192       # 2s × 4096 Hz
# SAMPLE_RATE = 4096        # Must match ESP32
# DTYPE = np.float32        # Must match ESP32
#
# # ---------------------------
# # Load WAV, trim or zero-pad + resample if needed
# # ---------------------------
# def load_wav_pcm(path, samples=NUM_SAMPLES, target_sr=SAMPLE_RATE):
#     data, sr = sf.read(path, dtype='float32')
#
#     # Resample if sample rate is different
#     if sr != target_sr:
#         print(f"Resampling {path}: {sr} Hz → {target_sr} Hz")
#         data = resampy.resample(data, sr, target_sr)
#         print("resampled")
#
#     # Neutral padding = silence
#     pcm = np.zeros(samples, dtype=DTYPE)
#
#     # Copy as much real audio as possible
#     length = min(len(data), samples)
#     pcm[:length] = data[:length]
#
#     return pcm
#
# # ---------------------------
# # Send audio and get result
# # ---------------------------
# def send_wav(ser, pcm):
#     ser.write(b"START\n")
#
#     # Wait for ESP32 handshake
#     while True:
#         line = ser.readline().decode(errors='ignore').strip()
#         if line == "STARTED":
#             break
#
#     # Send raw PCM (EXACT bytes)
#     ser.write(pcm.tobytes())
#
#     # Wait for prediction
#     while True:
#         line = ser.readline().decode(errors='ignore').strip()
#         if line.startswith("PREDICTION:"):
#             return int(line.split(":")[1])
#
# # ---------------------------
# # Main validation loop
# # ---------------------------
# def main():
#     anger_files = glob.glob("data/anger/*.wav")
#     other_files = glob.glob("data/other/*.wav")
#
#     wav_files = anger_files + other_files
#     labels = [1] * len(anger_files) + [0] * len(other_files)
#
#     if not wav_files:
#         print("No WAV files found in data/anger or data/other")
#         return
#
#     with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=10) as ser:
#         print("Waiting for ESP32...")
#         while True:
#             line = ser.readline().decode(errors='ignore').strip()
#             if line == "READY":
#                 print("ESP32 ready!")
#                 break
#
#         correct = 0
#
#         for i, (fpath, label) in enumerate(zip(wav_files, labels)):
#             pcm = load_wav_pcm(fpath)
#
#             # DEBUG safety check
#             assert pcm.nbytes == NUM_SAMPLES * 4
#
#             pred = send_wav(ser, pcm)
#
#             print(f"{i+1}/{len(wav_files)} {fpath} → pred={pred}, label={label}")
#
#             if pred == label:
#                 correct += 1
#
#             time.sleep(0.05)
#
#         acc = 100.0 * correct / len(wav_files)
#         print(f"\nValidation Accuracy: {acc:.2f}%")
#
# if __name__ == "__main__":
#     main()



import serial
import numpy as np
import soundfile as sf
import time
import os
import random

# ================= CONFIG =================
SERIAL_PORT = "COM9"        # change if needed
BAUD_RATE = 115200
SAMPLE_RATE = 16000        # MUST match kSampleRate in config.h
NUM_SAMPLES = 32000        # MUST match kNumSamples (e.g. 2 sec @16k)
DATA_DIR = "data"

# =========================================

def load_wav(path):
    audio, sr = sf.read(path, dtype="float32")

    if sr != SAMPLE_RATE:
        raise ValueError(f"{path}: expected {SAMPLE_RATE}, got {sr}")

    # mono
    if audio.ndim > 1:
        audio = audio.mean(axis=1)

    # pad / trim
    if len(audio) < NUM_SAMPLES:
        audio = np.pad(audio, (0, NUM_SAMPLES - len(audio)))
    else:
        audio = audio[:NUM_SAMPLES]

    return audio


def collect_files():
    files = []
    for label in ["anger", "other"]:
        folder = os.path.join(DATA_DIR, label)
        for f in os.listdir(folder):
            if f.endswith(".wav"):
                files.append((os.path.join(folder, f), label))
    random.shuffle(files)
    return files


def main():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
    time.sleep(2)

    files = collect_files()
    print(f"Found {len(files)} wav files")

    for i, (path, label) in enumerate(files, 1):
        print(f"\n[{i}/{len(files)}] Sending {path} ({label})")

        audio = load_wav(path)

        # Send start marker
        ser.write(b"START\n")
        time.sleep(0.05)

        # Send raw float32 PCM
        ser.write(audio.tobytes())

        # End marker
        ser.write(b"\nEND\n")

        # Read ESP32 output
        time.sleep(0.3)
        while ser.in_waiting:
            print(ser.readline().decode(errors="ignore").strip())

        # small pause between files
        time.sleep(0.5)


    print("\n✅ All files sent")


if __name__ == "__main__":
    main()
