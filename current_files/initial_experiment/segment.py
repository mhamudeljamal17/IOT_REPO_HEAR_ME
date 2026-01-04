import os
import librosa
import soundfile as sf

INPUT_DIR = "dataset"      # your current labeled data
OUTPUT_DIR = "segmented"   # new folder we will create

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Go over each emotion folder (e.g., neutral, angry)
for label in os.listdir(INPUT_DIR):
    input_folder = os.path.join(INPUT_DIR, label)
    if not os.path.isdir(input_folder):
        continue

    output_folder = os.path.join(OUTPUT_DIR, label)
    os.makedirs(output_folder, exist_ok=True)

    print(f"Processing label: {label}")

    for file in os.listdir(input_folder):
        if not file.endswith(".wav"):
            continue

        path = os.path.join(input_folder, file)

        # 1) Load audio at 16 kHz
        audio, sr = librosa.load(path, sr=16000)

        # 2) Define 1-second segment length
        segment_len = sr * 1   # 1 second

        idx = 0
        # 3) Slide over the file in 1-second steps
        for start in range(0, len(audio) - segment_len + 1, segment_len):
            segment = audio[start:start + segment_len]

            out_name = f"{file[:-4]}_{idx}.wav"
            out_path = os.path.join(output_folder, out_name)

            # 4) Save segment
            sf.write(out_path, segment, sr)
            idx += 1

    print(f"  Done with {label}")

print("All done! Check the 'segmented' folder.")
