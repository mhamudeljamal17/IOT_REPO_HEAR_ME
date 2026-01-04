from data_loader import DataLoader
from audio_processing import compute_mfcc_batch
from config import USE_LIBROSA_MFCC, MEL_BANDS

# Load training data only
data_loader = DataLoader("./data1")
train_audios, train_labels, _, _ = data_loader.load_and_split_data()

# Take just the first audio to inspect
sample_audio = train_audios[0:1]

# Compute MFCCs (as done in train.py)
mfccs = compute_mfcc_batch(sample_audio, use_librosa=USE_LIBROSA_MFCC)

print("MFCCs shape (train sample):", mfccs.shape)

# Flatten and show first 30 values for comparison with ESP32
mfcc_flat = mfccs[0].flatten()
print("First 30 MFCC values from training sample:")
print(mfcc_flat[:30])
