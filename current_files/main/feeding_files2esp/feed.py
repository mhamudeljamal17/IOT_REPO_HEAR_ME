import serial
import soundfile as sf
import numpy as np
import time
import glob
import librosa

PORT = "COM9"
BAUD = 115200
SAMPLE_RATE = 4096
BUFLEN = 4096*2  # Must match Arduino BUFLEN

ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(2)

# Folders and expected labels
folders = {
"./data/other/*.wav": "NOT ANGRY"
,    "./data/anger/*.wav": "ANGRY"

}

total = 0
correct = 0

for path_pattern, expected_label in folders.items():
    files = glob.glob(path_pattern)
    for wav in files:
        total += 1
        time.sleep(1)
        print("\nSending:", wav)

        # Load audio
        audio, sr = sf.read(wav)

        # Resample if needed
        if sr != SAMPLE_RATE:
            audio = librosa.resample(audio, orig_sr=sr, target_sr=SAMPLE_RATE)

        # Trim/pad to BUFLEN
        audio = audio[:BUFLEN]
        if len(audio) < BUFLEN:
            audio = np.pad(audio, (0, BUFLEN - len(audio)))

        # Convert to int16 PCM
        pcm = (audio * 32767).astype(np.int16)

        # Send START command
        ser.write(b"START\n")
        time.sleep(0.05)  # small delay for ESP32 to process command

        # Send audio samples
        for sample in pcm:
            ser.write(sample.tobytes())

        # Send END command
        ser.write(b"END\n")

        # Read ESP32 output until RESULT line
        predicted_label = None
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print("ESP32:", line)
                if "Predicted emotion" in line:
                    # Assuming your Arduino prints: "Predicted emotion: ANGRY" or "Predicted emotion: NOT ANGRY"
                    predicted_label = line.split(":")[-1].strip()
                    break

        if predicted_label == expected_label:
            correct += 1

# Print overall accuracy
accuracy = correct / total if total > 0 else 0
print(f"\nTotal files: {total}")
print(f"Correct predictions: {correct}")
print(f"Accuracy: {accuracy*100:.2f}%")
