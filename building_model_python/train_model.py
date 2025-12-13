# train_model.py
import tensorflow as tf
import numpy as np
N_MFCC=20
N_FRAMES=63

def build_model():
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(N_MFCC, N_FRAMES, 1)),

        tf.keras.layers.Conv2D(8, (3, 3), activation='relu'),
        tf.keras.layers.MaxPooling2D((2, 2)),

        tf.keras.layers.Conv2D(16, (3, 3), activation='relu'),
        tf.keras.layers.MaxPooling2D((2, 2)),

        tf.keras.layers.Flatten(),  # ← replaces RESHAPE safely
        tf.keras.layers.Dense(1, activation='sigmoid')
    ])

    # model = tf.keras.Sequential([
    #     tf.keras.layers.Input(shape=(20, 63, 1)),
    #
    #     tf.keras.layers.Conv2D(32, (3, 3), activation='relu'),
    #     tf.keras.layers.MaxPooling2D((2, 2)),
    #
    #     tf.keras.layers.Conv2D(16, (3, 3), activation='relu'),
    #     tf.keras.layers.Conv2D(16, (3, 3), activation='relu'),
    #     tf.keras.layers.MaxPooling2D((2, 2)),
    #
    #     tf.keras.layers.Flatten(),
    #
    #     tf.keras.layers.Dense(32, activation='relu'),
    #     tf.keras.layers.Dropout(0.36),
    #
    #     tf.keras.layers.Dense(32, activation='relu'),
    #     tf.keras.layers.Dropout(0.36),
    #
    #     tf.keras.layers.Dense(1, activation='sigmoid')
    # ])

    model.compile(
        optimizer=tf.keras.optimizers.Adam(0.00047),
        loss='binary_crossentropy',
        metrics=['accuracy']
    )

    return model


def train_model(model, X, y, epochs=150, batch_size=32):
    model.fit(
        X,
        y,
        epochs=epochs,
        batch_size=batch_size,
        shuffle=True
    )
    return model
