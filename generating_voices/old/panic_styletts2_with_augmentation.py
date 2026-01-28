import os
import torch
import soundfile as sf
import librosa
import numpy as np
from inference import infer

# =======================
# CONFIG
# =======================
REF_AUDIO = "myaudio.wav"
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

SENTENCES = [
    "Please open the door.",
    "I am waiting outside.",
    "The system is running normally.",
    "Everything is under control.",
    "Someone needs assistance."
]

N_SAMPLES = 20

# =======================
# CREATE DATASET FOLDERS
# =======================
os.makedirs("dataset_styletts2_with_augmentation/neutral", exist_ok=True)
os.makedirs("dataset_styletts2_with_augmentation/panic", exist_ok=True)

# =======================
# STYLE PARAMETERS
# =======================
NEUTRAL_STYLE = dict(alpha=0.95, beta=0.3, gamma=1.0)
PANIC_STYLE   = dict(alpha=0.9, beta=0.8, gamma=1.3)

# =======================
# AUGMENTATION
# =======================
def augment_panic(wav, sr):
    # Speed up
    wav = librosa.effects.time_stretch(wav, rate=1.25)

    # Pitch up
    wav = librosa.effects.pitch_shift(wav, sr=sr, n_steps=3)

    # Normalize
    wav = wav / np.max(np.abs(wav) + 1e-6)

    return wav

# =======================
# GENERATION
# =======================
counter = 0
for i in range(N_SAMPLES):
    text = SENTENCES[i % len(SENTENCES)]

    # Neutral (no augmentation)
    wav, sr = infer(
        text=text,
        ref_audio_path=REF_AUDIO,
        device=DEVICE,
        **NEUTRAL_STYLE
    )
    sf.write(f"dataset_styletts2_with_augmentation/neutral/neutral_{counter}.wav", wav, sr)

    # Panic + augmentation
    wav, sr = infer(
        text=text,
        ref_audio_path=REF_AUDIO,
        device=DEVICE,
        **PANIC_STYLE
    )
    wav = augment_panic(wav, sr)
    sf.write(f"dataset_styletts2_with_augmentation/panic/panic_{counter}.wav", wav, sr)

    counter += 1

print("StyleTTS2 dataset (WITH augmentation) created.")
