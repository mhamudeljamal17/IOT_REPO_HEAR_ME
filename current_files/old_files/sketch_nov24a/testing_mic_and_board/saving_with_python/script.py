import serial
import wave

ser = serial.Serial('COM9', 115200, timeout=5)
input("Press Enter to start recording...")
ser.write(b"rec\n")  # send rec command

raw_data = bytearray()
print("Receiving raw audio...")

# Read until no more data (you may define a fixed duration)
while True:
    chunk = ser.read(4096)
    if not chunk:
        break
    raw_data.extend(chunk)

# Save raw file
with open("audio.raw", "wb") as f:
    f.write(raw_data)

# Convert raw -> wav
with wave.open("audio.wav", "wb") as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(16000)
    wav_file.writeframes(raw_data)

print("Saved audio.wav")
