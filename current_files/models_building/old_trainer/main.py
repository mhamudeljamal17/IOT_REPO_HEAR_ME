# main.py
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split

from dataset_loader import load_dataset_from_roots
from train_model import build_model, train_model
from export_tflite import export_to_tflite
from convert_to_int8 import *
ROOT_FOLDERS = [
    "./data/AudioWAV_crema",
    "./data/research_data_esd"
]

H5_PATH = "anger_model_accuracy_computation.h5"
TFLITE_PATH = "../esp32_code/accuracy_computation.tflite"


def evaluate_int8_tflite(tflite_path, X_test, y_test):
    """Compute INT8 accuracy (ESP32-equivalent)"""

    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    in_scale, in_zero = input_details[0]["quantization"]
    out_scale, out_zero = output_details[0]["quantization"]

    correct = 0

    for i in range(len(X_test)):
        x = X_test[i]

        # Quantize input (same as ESP32)
        x_q = np.round(x / in_scale + in_zero).astype(np.int8)
        x_q = np.expand_dims(x_q, axis=0)

        interpreter.set_tensor(input_details[0]["index"], x_q)
        interpreter.invoke()

        raw = interpreter.get_tensor(output_details[0]["index"])[0][0]
        prob = (raw - out_zero) * out_scale

        pred = 1 if prob >= 0.5 else 0

        if pred == y_test[i]:
            correct += 1

    return correct / len(X_test)


def main():
    print("[INFO] Loading dataset...")
    X, y = load_dataset_from_roots(ROOT_FOLDERS)

    # -----------------------------
    # TRAIN / TEST SPLIT
    # -----------------------------
    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y,
        test_size=0.2,
        random_state=42,
        stratify=y
    )

    # Save test set for future analysis
    np.save("X_test.npy", X_test)
    np.save("y_test.npy", y_test)

    print("[INFO] Building model...")
    model = build_model()

    print("[INFO] Training...")
    model = train_model(model, X_train, y_train)

    # -----------------------------
    # FLOAT MODEL ACCURACY
    # -----------------------------
    loss, acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"✅ Float model accuracy: {acc:.4f}")

    model.save(H5_PATH)
    print(f"[INFO] Model saved to {H5_PATH}")

    # -----------------------------
    # EXPORT TO INT8 TFLITE
    # -----------------------------
    export_to_tflite(
        H5_PATH,
        TFLITE_PATH,
        X_train,  # representative data
        X_test,  # accuracy evaluation
        y_test
    )
    print(f"[INFO] TFLite model saved to {TFLITE_PATH}")

    # -----------------------------
    # INT8 (ESP32) ACCURACY
    # -----------------------------
    int8_acc = evaluate_int8_tflite(TFLITE_PATH, X_test, y_test)
    print(f"✅ INT8 (ESP32-equivalent) accuracy: {int8_acc:.4f}")

    print("✅ DONE")


if __name__ == "__main__":
    main()
