import tensorflow as tf
import numpy as np

def export_to_tflite(h5_path, tflite_path, n_mfcc=20, n_frames=63):
    """
    Converts a saved Keras model (.h5) to TFLite INT8 format for ESP32.
    """
    # 1) Load the trained model
    model = tf.keras.models.load_model(h5_path)
    print("[INFO] Loaded model from", h5_path)

    # 2) Create TFLite converter
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    # 3) Representative dataset generator
    def representative_data():
        for _ in range(100):
            sample = np.random.randn(n_mfcc, n_frames, 1).astype(np.float32)
            sample = np.expand_dims(sample, axis=0)  # batch dim
            yield [sample]

    converter.representative_dataset = representative_data
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    # 4) Convert
    tflite_model = converter.convert()

    # 5) Save TFLite file
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)

    print("[INFO] TFLite model saved to", tflite_path)


TFLITE_PATH = "anger_mode_int8.tflite"
H5_PATH = "panic_model2.h5"

export_to_tflite(H5_PATH, TFLITE_PATH)

