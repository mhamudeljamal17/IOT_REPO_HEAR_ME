"""
Prediction Module - Make Predictions on New Audio Files
"""

import numpy as np
from pathlib import Path
import tensorflow as tf
import librosa
from audio_processing import load_and_process_audio_librosa, compute_mfcc_batch
from config import EMOTIONS, MODELS_FOLDER, NUM_SAMPLES, SAMPLING_RATE, USE_LIBROSA_MFCC


class AudioPredictor:
    def __init__(self, model_path):
        self.model = tf.keras.models.load_model(model_path)
        self.model_name = Path(model_path).parent.name

    def predict_from_file(self, audio_file_path, threshold=0.5):
        """Predict emotion from an audio file"""

        # Load and process audio
        mfcc_features = load_and_process_audio_librosa(audio_file_path)

        if mfcc_features is None:
            return None

        # Add batch and channel dimensions
        mfcc_input = np.expand_dims(mfcc_features, axis=(0, -1))

        # Get prediction
        prediction = self.model.predict(mfcc_input, verbose=0)[0][0]

        # Binary classification
        predicted_class = 1 if prediction > threshold else 0
        emotion = EMOTIONS[predicted_class]
        confidence = prediction if predicted_class == 1 else 1 - prediction

        return {
            'emotion': emotion,
            'confidence': confidence,
            'raw_prediction': prediction,
            'predicted_class': predicted_class
        }

    def predict_batch(self, audio_files, threshold=0.5):
        """Predict emotions for multiple audio files"""

        results = []

        for audio_file in audio_files:
            result = self.predict_from_file(audio_file, threshold)
            if result:
                result['file'] = audio_file
                results.append(result)

        return results


def predict_on_folder(model_path, folder_path, threshold=0.5):
    """Predict emotions for all WAV files in a folder"""

    predictor = AudioPredictor(model_path)
    folder_path = Path(folder_path)

    wav_files = sorted(list(folder_path.glob("*.wav")))

    if not wav_files:
        print(f"No WAV files found in {folder_path}")
        return

    results = predictor.predict_batch(wav_files, threshold)

    if not results:
        print("No predictions were made")
        return

    print(f"\n{'=' * 80}")
    print(f"Predictions using {predictor.model_name}")
    print(f"{'=' * 80}")
    print(f"{'File':<50} {'Emotion':<15} {'Confidence':<15}")
    print(f"{'-' * 80}")

    emotion_counts = {'Angry': 0, 'Other': 0}

    for result in results:
        filename = Path(result['file']).name
        emotion = result['emotion']
        confidence = f"{result['confidence'] * 100:.2f}%"
        print(f"{filename:<50} {emotion:<15} {confidence:<15}")
        emotion_counts[emotion] += 1

    print(f"{'=' * 80}")
    print(f"Summary: {emotion_counts['Angry']} Angry, {emotion_counts['Other']} Other")
    print(f"{'=' * 80}\n")

    return results


if __name__ == "__main__":
    # Example usage
    model_path = str(Path(MODELS_FOLDER) / "MiniVGG16" / "MiniVGG16_final.keras")

    if Path(model_path).exists():
        # Predict on a folder
        # predict_on_folder(model_path, "./data/test/angry")
        pass