"""
Training Module - Train and Save Models
Updated with accuracy improvements from notebooks
"""

import os
import numpy as np
from pathlib import Path
import tensorflow as tf
from tensorflow.keras.callbacks import ModelCheckpoint, EarlyStopping, ReduceLROnPlateau
import matplotlib.pyplot as plt

from config import (
    EPOCHS, BATCH_SIZE, MODELS_FOLDER, MEL_BANDS,
    EMOTIONS, LEARNING_RATE, USE_LIBROSA_MFCC
)
from model_architectures import ModelArchitectures
from audio_processing import compute_mfcc_batch


class ModelTrainer:
    def __init__(self, model_name):
        self.model_name = model_name
        self.model = None
        self.history = None
        self.models_folder = Path(MODELS_FOLDER)
        self.models_folder.mkdir(exist_ok=True)

    def train(self, train_audios, train_labels, test_audios, test_labels):
        """Train a model with the given audio data"""

        print("\n" + "=" * 70)
        print(f"EXTRACTING MFCC FEATURES FOR {self.model_name}")
        print("=" * 70)

        # Compute MFCCs using librosa for best accuracy
        print("\nComputing training MFCCs...")
        train_mfccs = compute_mfcc_batch(train_audios, use_librosa=USE_LIBROSA_MFCC)

        print("\nComputing test MFCCs...")
        test_mfccs = compute_mfcc_batch(test_audios, use_librosa=USE_LIBROSA_MFCC)

        print(f"\nTraining MFCCs shape: {train_mfccs.shape}")
        print(f"Test MFCCs shape: {test_mfccs.shape}")

        # Add channel dimension
        train_mfccs = np.expand_dims(train_mfccs, axis=-1)
        test_mfccs = np.expand_dims(test_mfccs, axis=-1)

        # Get model architecture
        input_shape = (train_mfccs.shape[1], train_mfccs.shape[2], 1)
        self.model = self._get_model_architecture(input_shape)

        print("\n" + "=" * 70)
        print(f"TRAINING {self.model_name}")
        print("=" * 70)
        self.model.summary()

        # Create callbacks
        checkpoint_dir = self.models_folder / self.model_name / "checkpoints"
        checkpoint_dir.mkdir(parents=True, exist_ok=True)

        checkpoint_path = checkpoint_dir / "best_model_{epoch:02d}.h5"

        callbacks = [
            ModelCheckpoint(
                str(checkpoint_path),
                monitor='val_loss',
                save_best_only=True,
                mode='min',
                verbose=1
            ),
            EarlyStopping(
                monitor='val_loss',
                patience=25,  # Increased patience for better training
                restore_best_weights=True,
                verbose=1
            ),
            ReduceLROnPlateau(
                monitor='val_loss',
                factor=0.5,
                patience=10,
                min_lr=1e-7,
                verbose=1
            )
        ]

        # Train model
        print(f"\nStarting training with {len(train_audios)} samples...")
        self.history = self.model.fit(
            train_mfccs, train_labels,
            validation_data=(test_mfccs, test_labels),
            epochs=EPOCHS,
            batch_size=BATCH_SIZE,
            verbose=2,
            callbacks=callbacks
        )

        # Save final model in multiple formats
        model_dir = self.models_folder / self.model_name
        model_dir.mkdir(parents=True, exist_ok=True)

        # Save as .keras (recommended format)
        keras_path = model_dir / f"{self.model_name}_final.keras"
        self.model.save(str(keras_path))
        print(f"\nModel saved to {keras_path}")

        # Save as .h5 for compatibility
        h5_path = model_dir / f"{self.model_name}_final.h5"
        self.model.save(str(h5_path))
        print(f"Model also saved to {h5_path}")

        # Plot and save metrics
        self._plot_metrics()

        # Save training summary
        self._save_training_summary()

        return self.model, self.history

    def _get_model_architecture(self, input_shape):
        """Get the appropriate model architecture"""
        if self.model_name == "MiniVGG16":
            return ModelArchitectures.MiniVGG16(input_shape)
        elif self.model_name == "MobileNetV3Small":
            return ModelArchitectures.MobileNetV3Small(input_shape)
        elif self.model_name == "SqueezeNet":
            return ModelArchitectures.SqueezeNet(input_shape)
        else:
            raise ValueError(f"Unknown model: {self.model_name}")

    def _plot_metrics(self):
        """Plot and save training metrics"""
        if self.history is None:
            return

        loss = self.history.history['loss']
        val_loss = self.history.history['val_loss']
        accuracy = self.history.history['accuracy']
        val_accuracy = self.history.history['val_accuracy']

        fig, axes = plt.subplots(1, 2, figsize=(14, 5), dpi=300)

        # Loss plot
        axes[0].plot(loss, label='Training Loss', linewidth=2)
        axes[0].plot(val_loss, label='Validation Loss', linewidth=2)
        axes[0].set_xlabel('Epoch', fontsize=12)
        axes[0].set_ylabel('Loss', fontsize=12)
        axes[0].set_title('Loss over Epochs', fontsize=14, fontweight='bold')
        axes[0].legend()
        axes[0].grid(True, linestyle='--', alpha=0.3)

        # Accuracy plot
        axes[1].plot(accuracy, label='Training Accuracy', linewidth=2)
        axes[1].plot(val_accuracy, label='Validation Accuracy', linewidth=2)
        axes[1].set_xlabel('Epoch', fontsize=12)
        axes[1].set_ylabel('Accuracy', fontsize=12)
        axes[1].set_title('Accuracy over Epochs', fontsize=14, fontweight='bold')
        axes[1].legend()
        axes[1].grid(True, linestyle='--', alpha=0.3)

        plt.suptitle(f'{self.model_name} - Training Metrics', fontsize=16, fontweight='bold')
        plt.tight_layout()

        plot_path = self.models_folder / self.model_name / "training_metrics.png"
        plot_path.parent.mkdir(parents=True, exist_ok=True)
        plt.savefig(str(plot_path), dpi=300, bbox_inches='tight')
        print(f"Metrics plot saved to {plot_path}")
        plt.close()

    def _save_training_summary(self):
        """Save training summary to text file"""
        summary_path = self.models_folder / self.model_name / "training_summary.txt"

        if self.history is None or not self.history.history:
            return

        best_val_accuracy = max(self.history.history['val_accuracy'])
        best_epoch = np.argmax(self.history.history['val_accuracy']) + 1
        final_train_accuracy = self.history.history['accuracy'][-1]
        final_val_accuracy = self.history.history['val_accuracy'][-1]

        with open(summary_path, 'w') as f:
            f.write(f"Training Summary for {self.model_name}\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Total Epochs: {len(self.history.history['accuracy'])}\n")
            f.write(f"Best Validation Accuracy: {best_val_accuracy:.4f} (Epoch {best_epoch})\n")
            f.write(f"Final Training Accuracy: {final_train_accuracy:.4f}\n")
            f.write(f"Final Validation Accuracy: {final_val_accuracy:.4f}\n")
            f.write(f"Total Parameters: {self.model.count_params()}\n")

        print(f"Training summary saved to {summary_path}")


def train_all_models(train_audios, train_labels, test_audios, test_labels):
    """Train all three models"""
    model_names = ["MiniVGG16", "MobileNetV3Small", "SqueezeNet"]

    for model_name in model_names:
        print(f"\n{'=' * 70}")
        print(f"Training {model_name}")
        print(f"{'=' * 70}")

        trainer = ModelTrainer(model_name)
        trainer.train(train_audios, train_labels, test_audios, test_labels)


if __name__ == "__main__":
    from data_loader import DataLoader

    # Load and split data
    data_loader = DataLoader("./data")
    train_audios, train_labels, test_audios, test_labels = data_loader.load_and_split_data()

    if train_audios is not None:
        # Train all models
        train_all_models(train_audios, train_labels, test_audios, test_labels)

        print("\n" + "=" * 70)
        print("All models trained successfully!")
        print("=" * 70)
    else:
        print("Failed to load data!")