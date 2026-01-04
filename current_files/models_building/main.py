"""
Main Script - Complete Pipeline for Training and Evaluation
Updated for automatic data loading from multiple datasets
"""

import sys
from pathlib import Path
import numpy as np

from config import MODELS_FOLDER, USE_LIBROSA_MFCC
from data_loader import DataLoader
from train import ModelTrainer, train_all_models
from evaluate import ModelEvaluator
from audio_processing import compute_mfcc_batch


def main():
    print("\n" + "=" * 70)
    print("ANGRY EMOTION DETECTION SYSTEM - Complete Pipeline")
    print("=" * 70)
    print(f"Using {'Librosa' if USE_LIBROSA_MFCC else 'Custom'} MFCC computation")
    print("=" * 70 + "\n")

    # Step 1: Load and Split Data
    print("STEP 1: Loading and Splitting Data")
    print("-" * 70)
    data_loader = DataLoader("./data")
    train_audios, train_labels, test_audios, test_labels = data_loader.load_and_split_data()

    if train_audios is None or len(train_audios) == 0:
        print("ERROR: Could not load data. Please check the data folder structure.")
        print("\nExpected structure:")
        print("  ./data/AudioWAV_crema/angry/*.wav")
        print("  ./data/AudioWAV_crema/other/*.wav")
        print("  ./data/research_data_esd/angry/*.wav")
        print("  ./data/research_data_esd/other/*.wav")
        print("  ... (any number of datasets)")
        sys.exit(1)

    # Step 2: Train Models
    print("\n" + "=" * 70)
    print("STEP 2: Training Models")
    print("-" * 70)
    train_all_models(train_audios, train_labels, test_audios, test_labels)

    # Step 3: Compute Test MFCCs for Evaluation
    print("\n" + "=" * 70)
    print("STEP 3: Computing Test Features for Evaluation")
    print("-" * 70)
    print("\nComputing test MFCCs...")
    test_mfccs = compute_mfcc_batch(test_audios, use_librosa=USE_LIBROSA_MFCC)

    # Step 4: Evaluate Models
    print("\n" + "=" * 70)
    print("STEP 4: Evaluating Models on Test Set")
    print("-" * 70)

    model_names = ["MobileNetV3Small","MiniVGG16", "SqueezeNet"]
    evaluation_results = {}

    for model_name in model_names:
        model_path = Path(MODELS_FOLDER) / model_name / f"{model_name}_final.keras"

        if model_path.exists():
            print(f"\nEvaluating {model_name}...")
            evaluator = ModelEvaluator(str(model_path))
            results = evaluator.evaluate(test_mfccs, test_labels)
            evaluator.print_metrics(results)
            evaluation_results[model_name] = results
        else:
            print(f"Model {model_path} not found!")

    # Step 5: Summary and Comparison
    print("\n" + "=" * 70)
    print("MODEL COMPARISON - FINAL RESULTS")
    print("=" * 70)

    if evaluation_results:
        print(f"\n{'Model':<20} {'Accuracy':<12} {'Precision':<12} {'Recall':<12} {'F1-Score':<12} {'ROC-AUC':<12}")
        print("-" * 80)

        best_accuracy = 0
        best_model = ""

        for model_name, results in evaluation_results.items():
            accuracy = results['accuracy']
            if accuracy > best_accuracy:
                best_accuracy = accuracy
                best_model = model_name

            print(f"{model_name:<20} {accuracy:<12.4f} {results['precision']:<12.4f} "
                  f"{results['recall']:<12.4f} {results['f1']:<12.4f} {results['roc_auc']:<12.4f}")

        print("-" * 80)
        print(f"\nBest Model: {best_model} with {best_accuracy:.4f} accuracy")

    print("\n" + "=" * 70)
    print("Pipeline completed successfully!")
    print(f"Models saved to: {MODELS_FOLDER}")
    print("=" * 70 + "\n")


if __name__ == "__main__":
    main()