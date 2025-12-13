# export_tflite.py
import tensorflow as tf
import os

def export_to_tflite(h5_path, tflite_path):
    model = tf.keras.models.load_model(h5_path)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]

    tflite_model = converter.convert()

    os.makedirs(os.path.dirname(tflite_path), exist_ok=True)

    with open(tflite_path, "wb") as f:
        f.write(tflite_model)
