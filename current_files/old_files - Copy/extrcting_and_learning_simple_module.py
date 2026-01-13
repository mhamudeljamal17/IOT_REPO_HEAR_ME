import librosa
import numpy as np
import glob
import os
import tensorflow as tf

# ================= DATASET =================
dataset_path = os.path.join("..", "dataset")

def extract_mfcc(file_path, n_mfcc=40, sr=16000):
    y, _ = librosa.load(file_path, sr=sr, mono=True)
    mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=n_mfcc)
    return np.mean(mfcc.T, axis=0)

X = []
Y = []

neutral_folder = os.path.join(dataset_path, "Other")
emergency_folder = os.path.join(dataset_path, "angry")

for file in glob.glob(os.path.join(neutral_folder, "*.wav")):
    X.append(extract_mfcc(file))
    Y.append(0)

for file in glob.glob(os.path.join(emergency_folder, "*.wav")):
    X.append(extract_mfcc(file))
    Y.append(1)

X = np.array(X, dtype=np.float32)
Y = np.array(Y, dtype=np.float32)

print("Features shape:", X.shape)
print("Labels shape:", Y.shape)

# ================= MODEL =================
model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(40,)),
    tf.keras.layers.Dense(64, activation='relu'),
    tf.keras.layers.Dense(32, activation='relu'),
    tf.keras.layers.Dense(1, activation='sigmoid')  # binary classification
])

model.compile(optimizer='adam', loss='binary_crossentropy', metrics=['accuracy'])
model.fit(X, Y, epochs=50, batch_size=8, validation_split=0.1)
model.save("emotion_binary.h5")

# ================= TFLITE CONVERSION WITH INT8 =================
def representative_dataset_gen():
    for i in range(len(X)):
        # reshape to (1,40) and convert to float32
        sample = X[i].reshape(1, 40)
        yield [sample]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()
with open("emotion_binary_int8.tflite", "wb") as f:
    f.write(tflite_model)

# ================= EXPORT TO model_data.h =================
tflite_file = "emotion_binary_int8.tflite"
c_file = "model_data.h"

with open(tflite_file, "rb") as f:
    data = f.read()

with open(c_file, "w") as f:
    f.write("unsigned char model_data[] = {")
    for i, b in enumerate(data):
        if i % 12 == 0:
            f.write("\n  ")
        f.write(f"0x{b:02x}, ")
    f.write("\n};\n")
    f.write(f"unsigned int model_data_len = {len(data)};\n")

print("✅ model_data.h generated successfully!")
