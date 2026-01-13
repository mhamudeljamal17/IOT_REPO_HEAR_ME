import wave

# Parameters
input_file = "audio.raw"
output_file = "audiowav.wav"
channels = 1          # mono
sample_width = 2      # bytes per sample (16-bit)
sample_rate = 16000   # Hz

# Read raw data
with open(input_file, "rb") as f:
    raw_data = f.read()

# Write WAV
with wave.open(output_file, "wb") as wf:
    wf.setnchannels(channels)
    wf.setsampwidth(sample_width)
    wf.setframerate(sample_rate)
    wf.writeframes(raw_data)

print(f"Converted {input_file} → {output_file}")
