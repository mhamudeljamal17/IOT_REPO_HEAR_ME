# main.py
import numpy as np
from dataset_loader import load_dataset_from_roots
from train_model import build_model, train_model
from export_tflite import export_to_tflite

ROOT_FOLDERS = [
    "./data/AudioWAV_crema",
    "./data/research_data_esd"
]

H5_PATH = "panic_model2.h5"
TFLITE_PATH = "../esp32_code/panic_model2.tflite"


def main():
    print("[INFO] Loading dataset...")
    X, y = load_dataset_from_roots(ROOT_FOLDERS)

    print("[INFO] Building model...")
    model = build_model()

    print("[INFO] Training...")
    model = train_model(model, X, y)

    model.save(H5_PATH)
    print(f"[INFO] Model saved to {H5_PATH}")

    export_to_tflite(H5_PATH, TFLITE_PATH)


    print("✅ DONE")


if __name__ == "__main__":
    main()
