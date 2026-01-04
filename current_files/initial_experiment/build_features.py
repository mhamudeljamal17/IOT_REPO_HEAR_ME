import os
import numpy as np
import librosa
from sklearn.preprocessing import LabelEncoder
import joblib  # to save the label encoder

DATA_DIR = "segmented"   # our 1-second clips
SAMPLE_RATE = 16000
N_MFCC = 40
MAX_FRAMES = 50          # we will pad/truncate MFCCs to 50 frames

def extract_mfcc(file_path):
    # Load audio
    audio, sr = librosa.load(file_path, sr=SAMPLE_RATE)

    # Compute MFCCs: shape (n_mfcc, time_frames)
    mfcc = librosa.feature.mfcc(
        y=audio,
        sr=sr,
        n_mfcc=N_MFCC
    )

    # Pad or cut to fixed length (MAX_FRAMES)
    if mfcc.shape[1] < MAX_FRAMES:
        pad_width = MAX_FRAMES - mfcc.shape[1]
        mfcc = np.pad(mfcc, ((0, 0), (0, pad_width)), mode='constant')
    else:
        mfcc = mfcc[:, :MAX_FRAMES]

    # Add channel dimension for Conv2D later: (40, 50, 1)
    mfcc = mfcc[..., np.newaxis]
    return mfcc

labels = []
features = []

# Go over each label folder, e.g. segmented/neutral, segmented/angry
for label_name in os.listdir(DATA_DIR):
    label_folder = os.path.join(DATA_DIR, label_name)
    if not os.path.isdir(label_folder):
        continue

    print(f"Processing label: {label_name}")

    for file in os.listdir(label_folder):
        if not file.endswith(".wav"):
            continue

        path = os.path.join(label_folder, file)
        mfcc = extract_mfcc(path)

        features.append(mfcc)
        labels.append(label_name)

features = np.array(features)   # shape: (N, 40, 50, 1)
labels = np.array(labels)

print("Features shape:", features.shape)
print("Labels shape:", labels.shape)

# Encode text labels ("neutral", "angry") → 0, 1
encoder = LabelEncoder()
y_encoded = encoder.fit_transform(labels)

print("Classes:", encoder.classes_)
print("Encoded labels example:", y_encoded[:10])

# Save everything
np.save("X.npy", features)
np.save("y.npy", y_encoded)
joblib.dump(encoder, "label_encoder.pkl")

print("Saved X.npy, y.npy and label_encoder.pkl")
