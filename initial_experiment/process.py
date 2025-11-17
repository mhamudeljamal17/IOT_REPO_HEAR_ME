import os
import shutil

# Emotion code → label
emotion_map = {
    "01": "neutral",
    "02": "calm",
    "03": "happy",
}

input_dir = "ravdess"
output_dir = "dataset"

os.makedirs(output_dir, exist_ok=True)

for root, dirs, files in os.walk(input_dir):
    for file in files:
        if not file.endswith(".wav"):
            continue

        parts = file.split("-")
        emotion_code = parts[2]   # third number in filename

        if emotion_code in emotion_map:
            label = "neutral"
        else:
            label = "angry"

        label_folder = os.path.join(output_dir, label)
        os.makedirs(label_folder, exist_ok=True)

        src = os.path.join(root, file)
        dst = os.path.join(label_folder, file)

        shutil.copy(src, dst)

print("Sorting done! Check the 'dataset' folder.")
