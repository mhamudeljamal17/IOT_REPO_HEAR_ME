# export_tflite.py
import tensorflow as tf
import numpy as np


def export_to_tflite(
    h5_path,
    tflite_path,
    X_rep,
    X_test,
    y_test,
    threshold=0.5
):
    """
    Converts a Keras model to INT8 TFLite and
    computes ESP32-equivalent INT8 accuracy.
    """

    # ==============================
    # 1️⃣ Load trained model
    # ==============================
    model = tf.keras.models.load_model(h5_path)
    print("[INFO] Loaded model from", h5_path)

    # ==============================
    # 2️⃣ Create converter
    # ==============================
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    # ==============================
    # 3️⃣ Representative dataset (REAL MFCCs)
    # ==============================
    def representative_dataset():
        for i in range(min(200, len(X_rep))):
            sample = X_rep[i].astype(np.float32)
            sample = np.expand_dims(sample, axis=0)
            yield [sample]

    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # ==============================
    # 4️⃣ Convert & save
    # ==============================
    tflite_model = converter.convert()

    with open(tflite_path, "wb") as f:
        f.write(tflite_model)

    print("[INFO] INT8 TFLite model saved to", tflite_path)

    # ==============================
    # 5️⃣ Evaluate INT8 accuracy
    # ==============================
    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    in_scale, in_zero = input_details[0]["quantization"]
    out_scale, out_zero = output_details[0]["quantization"]

    correct = 0

    for i in range(len(X_test)):
        x = X_test[i]

        # Quantize input (ESP32-style)
        x_q = np.round(x / in_scale + in_zero).astype(np.int8)
        x_q = np.expand_dims(x_q, axis=0)

        interpreter.set_tensor(input_details[0]["index"], x_q)
        interpreter.invoke()

        raw = interpreter.get_tensor(output_details[0]["index"])[0][0]
        prob = (raw - out_zero) * out_scale

        pred = 1 if prob >= threshold else 0

        if pred == y_test[i]:
            correct += 1

    int8_accuracy = correct / len(X_test)

    print(f"✅ INT8 (ESP32-equivalent) accuracy: {int8_accuracy:.4f}")

    return int8_accuracy

TFLITE_PATH = "anger_mode_int8_accuracy_computation.tflite"
H5_PATH = "panic_model_accuracy_computation.h5"

#export_to_tflite(H5_PATH, TFLITE_PATH)

