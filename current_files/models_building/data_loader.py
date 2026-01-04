"""
Data Loader Module - Load all data from two datasets and split automatically
Handles structure: data/dataset1/{angry,other} and data/dataset2/{angry,other}
"""

import os
import numpy as np
import librosa
from pathlib import Path
from sklearn.model_selection import train_test_split
from config import (
    SAMPLING_RATE, DURATION, NUM_SAMPLES, EMOTIONS,
    DATA_FOLDER, TRAIN_TEST_SPLIT, RANDOM_SEED
)
from audio_processing import load_and_process_audio_librosa


class DataLoader:
    def __init__(self, data_folder):
        self.data_folder = Path(data_folder)
        self.sampling_rate = SAMPLING_RATE
        self.duration = DURATION

    def load_audio_from_folder(self, folder_path, label):
        """Load all WAV files from a folder and assign a label"""
        folder_path = Path(folder_path)

        if not folder_path.exists():
            print(f"Warning: Folder {folder_path} does not exist!")
            return np.array([]), np.array([])

        wav_files = sorted(folder_path.glob("*.wav"))
        print(f"  Found {len(wav_files)} WAV files in {folder_path.name}")

        audios = []
        labels = []

        for file_path in wav_files:
            try:
                # Load audio
                audio, sr = librosa.load(str(file_path), sr=self.sampling_rate,
                                         mono=True, duration=self.duration)

                # Pad or truncate
                if audio.shape[0] < NUM_SAMPLES:
                    audio = np.append(audio, np.zeros(NUM_SAMPLES - audio.shape[0]))
                else:
                    audio = audio[:NUM_SAMPLES]

                audios.append(audio)
                labels.append(label)

            except Exception as e:
                print(f"    Error loading {file_path.name}: {e}")

        return np.asarray(audios), np.asarray(labels)

    def load_all_datasets(self):
        """
        Load all data from data folder structure:
        data/dataset1/angry/
        data/dataset1/other/
        data/dataset2/angry/
        data/dataset2/other/
        ... (any number of datasets)
        """
        print("\n" + "=" * 70)
        print("LOADING ALL DATASETS")
        print("=" * 70)

        all_audios = []
        all_labels = []

        # Find all dataset folders
        data_path = Path(self.data_folder)

        if not data_path.exists():
            print(f"Error: Data folder {data_path} does not exist!")
            return np.array([]), np.array([])

        # Get all subdirectories (datasets)
        dataset_folders = sorted([d for d in data_path.iterdir() if d.is_dir()])

        if not dataset_folders:
            print("Error: No dataset folders found!")
            return np.array([]), np.array([])

        total_audios_loaded = 0

        for dataset_folder in dataset_folders:
            print(f"\nProcessing {dataset_folder.name}:")

            # Load angry emotion (label = 1)
            angry_path = dataset_folder / "angry"
            print(f"  Loading angry emotion:")
            angry_audios, angry_labels = self.load_audio_from_folder(angry_path, 1)

            # Load other emotions (label = 0)
            other_path = dataset_folder / "other"
            print(f"  Loading other emotions:")
            other_audios, other_labels = self.load_audio_from_folder(other_path, 0)

            # Combine dataset
            if len(angry_audios) > 0 and len(other_audios) > 0:
                dataset_audios = np.concatenate([angry_audios, other_audios])
                dataset_labels = np.concatenate([angry_labels, other_labels])
                all_audios.append(dataset_audios)
                all_labels.append(dataset_labels)
                total_audios_loaded += len(dataset_audios)
                print(f"  Total from {dataset_folder.name}: {len(dataset_audios)} audios")

        if not all_audios:
            print("Error: No audio files were loaded!")
            return np.array([]), np.array([])

        # Concatenate all datasets
        combined_audios = np.concatenate(all_audios)
        combined_labels = np.concatenate(all_labels)

        # Shuffle combined data
        shuffle_idx = np.random.RandomState(RANDOM_SEED).permutation(len(combined_audios))
        combined_audios = combined_audios[shuffle_idx]
        combined_labels = combined_labels[shuffle_idx]

        print(f"\n{'=' * 70}")
        print(f"TOTAL AUDIOS LOADED: {total_audios_loaded}")
        print(f"  - Angry: {np.sum(combined_labels == 1)}")
        print(f"  - Other: {np.sum(combined_labels == 0)}")
        print(f"{'=' * 70}\n")

        return combined_audios, combined_labels

    def load_and_split_data(self):
        """Load all data and split into train/test sets"""

        # Load all data
        all_audios, all_labels = self.load_all_datasets()

        if len(all_audios) == 0:
            return None, None, None, None

        # Split into train and test
        train_audios, test_audios, train_labels, test_labels = train_test_split(
            all_audios, all_labels,
            test_size=1 - TRAIN_TEST_SPLIT,
            random_state=RANDOM_SEED,
            stratify=all_labels  # Ensure balanced split
        )

        print("=" * 70)
        print("DATA SPLIT SUMMARY")
        print("=" * 70)
        print(f"\nTraining Set: {len(train_audios)} audios")
        print(f"  - Angry: {np.sum(train_labels == 1)}")
        print(f"  - Other: {np.sum(train_labels == 0)}")

        print(f"\nTest Set: {len(test_audios)} audios")
        print(f"  - Angry: {np.sum(test_labels == 1)}")
        print(f"  - Other: {np.sum(test_labels == 0)}")
        print(f"{'=' * 70}\n")

        return train_audios, train_labels, test_audios, test_labels