import tensorflow as tf

# Load your Keras model
model = tf.keras.models.load_model("MiniVGG16_final.keras")

# Convert to TFLite (Float32)
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

# Save TFLite model
tflite_path = "MiniVGG16_final.tflite"
with open(tflite_path, "wb") as f:
    f.write(tflite_model)

print(f"TFLite model saved to {tflite_path}")
