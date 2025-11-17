import numpy as np
from sklearn.model_selection import train_test_split
from tensorflow.keras import layers, models
import joblib

# Load data
X = np.load("X.npy")   # shape: (N, 40, 50, 1)
y = np.load("y.npy")   # shape: (N,)

encoder = joblib.load("label_encoder.pkl")
num_classes = len(encoder.classes_)

print("Data shape:", X.shape)
print("Number of classes:", num_classes, encoder.classes_)

# Train / test split
X_train, X_val, y_train, y_val = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

# Build a small CNN
model = models.Sequential([
    layers.Conv2D(16, (3, 3), activation="relu", input_shape=(40, 50, 1)),
    layers.MaxPooling2D((2, 2)),

    layers.Conv2D(32, (3, 3), activation="relu"),
    layers.MaxPooling2D((2, 2)),

    layers.Flatten(),
    layers.Dense(64, activation="relu"),
    layers.Dense(num_classes, activation="softmax"),
])

model.compile(
    optimizer="adam",
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"],
)

model.summary()

# Train
history = model.fit(
    X_train, y_train,
    epochs=15,
    batch_size=32,
    validation_data=(X_val, y_val)
)

# Evaluate
val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
print(f"Validation accuracy: {val_acc:.3f}")

# Save model
model.save("tone_model.h5")
print("Saved model as tone_model.h5")
