"""
Evaluation Module - Evaluate Model Performance
"""

import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
from sklearn.metrics import (confusion_matrix, precision_recall_fscore_support,
                             roc_curve, auc, roc_auc_score, classification_report)
import tensorflow as tf

from config import EMOTIONS, MODELS_FOLDER, MEL_BANDS
from audio_processing import compute_mfcc_batch


class ModelEvaluator:
    def __init__(self, model_path):
        self.model = tf.keras.models.load_model(model_path)
        self.model_path = Path(model_path)
        self.model_name = self.model_path.parent.name

    def evaluate(self, test_mfccs, test_labels, threshold=0.5):
        """Evaluate model on test data"""

        # Add channel dimension if needed
        if len(test_mfccs.shape) == 3:
            test_mfccs = np.expand_dims(test_mfccs, axis=-1)

        # Get predictions
        predictions = self.model.predict(test_mfccs, verbose=0)
        predictions = predictions.squeeze()

        # Binary predictions
        predicted_labels = (predictions > threshold).astype(int)

        # Calculate metrics
        accuracy = np.mean(predicted_labels == test_labels)
        precision, recall, f1, _ = precision_recall_fscore_support(
            test_labels, predicted_labels, average='weighted'
        )

        # Confusion matrix
        cm = confusion_matrix(test_labels, predicted_labels)

        # ROC curve
        fpr, tpr, _ = roc_curve(test_labels, predictions)
        roc_auc = auc(fpr, tpr)

        # Per-class metrics
        class_report = classification_report(test_labels, predicted_labels,
                                             target_names=EMOTIONS, output_dict=True)

        results = {
            'accuracy': accuracy,
            'precision': precision,
            'recall': recall,
            'f1': f1,
            'confusion_matrix': cm,
            'fpr': fpr,
            'tpr': tpr,
            'roc_auc': roc_auc,
            'predictions': predictions,
            'predicted_labels': predicted_labels,
            'class_report': class_report
        }

        return results

    def plot_confusion_matrix(self, cm, save_path=None):
        """Plot confusion matrix"""
        fig, ax = plt.subplots(figsize=(8, 6), dpi=300)

        cm_df = pd.DataFrame(cm, index=EMOTIONS, columns=EMOTIONS)
        sns.heatmap(cm_df, annot=True, fmt='d', cmap='Blues', ax=ax,
                    cbar_kws={'label': 'Count'}, annot_kws={'size': 14, 'weight': 'bold'})

        ax.set_xlabel('Predicted', fontsize=12, fontweight='bold')
        ax.set_ylabel('Actual', fontsize=12, fontweight='bold')
        ax.set_title(f'{self.model_name} - Confusion Matrix', fontsize=14, fontweight='bold')

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Confusion matrix saved to {save_path}")

        plt.show()

    def plot_roc_curve(self, fpr, tpr, roc_auc, save_path=None):
        """Plot ROC curve"""
        fig, ax = plt.subplots(figsize=(8, 6), dpi=300)

        ax.plot(fpr, tpr, color='blue', lw=2, label=f'ROC (AUC = {roc_auc:.4f})')
        ax.plot([0, 1], [0, 1], color='gray', linestyle='--', lw=1)

        ax.set_xlim([0.0, 1.0])
        ax.set_ylim([0.0, 1.05])
        ax.set_xlabel('False Positive Rate', fontsize=12)
        ax.set_ylabel('True Positive Rate', fontsize=12)
        ax.set_title(f'{self.model_name} - ROC Curve', fontsize=14, fontweight='bold')
        ax.legend(loc='lower right', fontsize=11)
        ax.grid(True, linestyle='--', alpha=0.3)

        plt.tight_layout()

        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"ROC curve saved to {save_path}")

        plt.show()

    def print_metrics(self, results):
        """Print evaluation metrics"""
        print(f"\n{'=' * 70}")
        print(f"{self.model_name} - Evaluation Metrics")
        print(f"{'=' * 70}")
        print(f"Accuracy:  {results['accuracy']:.4f}")
        print(f"Precision: {results['precision']:.4f}")
        print(f"Recall:    {results['recall']:.4f}")
        print(f"F1-Score:  {results['f1']:.4f}")
        print(f"ROC AUC:   {results['roc_auc']:.4f}")

        print(f"\nPer-Class Metrics:")
        print("-" * 70)
        print(f"{'Class':<15} {'Precision':<15} {'Recall':<15} {'F1-Score':<15}")
        print("-" * 70)
        for class_name in EMOTIONS:
            metrics = results['class_report'][class_name]
            print(
                f"{class_name:<15} {metrics['precision']:<15.4f} {metrics['recall']:<15.4f} {metrics['f1-score']:<15.4f}")
        print(f"{'=' * 70}\n")


if __name__ == "__main__":
    from data_loader import DataLoader

    # Load and split data
    data_loader = DataLoader("./data")
    train_audios, train_labels, test_audios, test_labels = data_loader.load_and_split_data()

    if test_audios is not None:
        # Compute test MFCCs
        print("\nComputing test features for evaluation...")
        test_mfccs = compute_mfcc_batch(test_audios)

        # Evaluate each model
        model_names = ["MiniVGG16", "MobileNetV3Small", "SqueezeNet"]

        for model_name in model_names:
            model_path = Path(MODELS_FOLDER) / model_name / f"{model_name}_final.keras"

            if model_path.exists():
                print(f"\nEvaluating {model_name}...")
                evaluator = ModelEvaluator(str(model_path))
                results = evaluator.evaluate(test_mfccs, test_labels)
                evaluator.print_metrics(results)

                # Plot confusion matrix
                cm_path = Path(MODELS_FOLDER) / model_name / "confusion_matrix.png"
                evaluator.plot_confusion_matrix(results['confusion_matrix'], str(cm_path))

                # Plot ROC curve
                roc_path = Path(MODELS_FOLDER) / model_name / "roc_curve.png"
                evaluator.plot_roc_curve(results['fpr'], results['tpr'],
                                         results['roc_auc'], str(roc_path))
            else:
                print(f"Model {model_path} not found!")