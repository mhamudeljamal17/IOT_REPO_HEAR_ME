import tensorflow as tf
import numpy as np

model = tf.keras.models.load_model("MiniVGG16_final.h5")

def representative_dataset():
    for _ in range(100):
        data = np.random.rand(1, 20, 63, 1).astype(np.float32)
        yield [data]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset

converter.target_spec.supported_ops = [
    tf.lite.OpsSet.TFLITE_BUILTINS_INT8
]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

with open("model_int8.tflite", "wb") as f:
    f.write(tflite_model)

print("INT8 model size:", len(tflite_model) / 1024, "KB")
