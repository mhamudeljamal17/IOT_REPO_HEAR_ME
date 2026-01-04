import os
import numpy as np
import soundfile as sf
from mfcc import compute_mfcc

TARGET_SAMPLES = 4096 * 2
TARGET_FRAMES = 63

LABELS = {
    "angry": 1,
    "other": 0
}

def _load_class(folder, label):
    X, y = [], []

    for file in os.listdir(folder):
        if not file.endswith(".wav"):
            continue

        path = os.path.join(folder, file)
        signal, fs = sf.read(path)

        # ---- Enforce fixed signal length ----
        signal = signal[:TARGET_SAMPLES]
        if len(signal) < TARGET_SAMPLES:
            pad = np.random.normal(0, 0.01 * np.var(signal),
                                   TARGET_SAMPLES - len(signal))
            signal = np.concatenate([signal, pad])

        # ---- MFCC ----
        mfcc = compute_mfcc(signal)  # (20, T)

        # ---- Enforce fixed MFCC time dimension ----
        if mfcc.shape[1] < TARGET_FRAMES:
            mfcc = np.pad(
                mfcc,
                ((0, 0), (0, TARGET_FRAMES - mfcc.shape[1])),
                mode="constant"
            )
        else:
            mfcc = mfcc[:, :TARGET_FRAMES]

        X.append(mfcc[..., np.newaxis])  # (20, 63, 1)
        y.append(label)

    return X, y


def load_dataset_from_roots(root_folders):
    """
    root_folders: list of paths, each containing:
        angry/
        other/
    """
    X_all, y_all = [], []

    for root in root_folders:
        for class_name, label in LABELS.items():
            class_dir = os.path.join(root, class_name)

            if not os.path.isdir(class_dir):
                raise ValueError(f"Missing folder: {class_dir}")

            X, y = _load_class(class_dir, label)
            X_all.extend(X)
            y_all.extend(y)

    # ---- Convert lists to NumPy arrays ----
    X_all = np.array(X_all, dtype=np.float32)
    y_all = np.array(y_all, dtype=np.float32)

    return X_all, y_all
