"""
Configuration file for the Angry Emotion Detection System
Updated for automatic train/test split from two datasets
"""

# Audio Processing Parameters
SAMPLING_RATE = 4096  # Hz
DURATION = 2  # seconds
NUM_SAMPLES = SAMPLING_RATE * DURATION  # 8192

# MFCC Computation Parameters
SIZE_WIN = 256
SIZE_OFF = 128
TOTAL_WIN_NUM = int(((NUM_SAMPLES - SIZE_WIN) / SIZE_OFF) + 1)
FREQ_MIN = 20
FREQ_MAX = SAMPLING_RATE / 2
MEL_BANDS = 20

# Model Training Parameters
EPOCHS = 150
BATCH_SIZE = 32
LEARNING_RATE = 0.00047
DROPOUT_RATE = 0.36
L2_REGULARIZATION = 0.57

# Data Paths
DATA_FOLDER = "./data"
MODELS_FOLDER = "./models"

# Classes
CLASSES = [0, 1]
EMOTIONS = ["Other", "Angry"]

# Data Split
TRAIN_TEST_SPLIT = 0.8  # 80% training, 20% testing
RANDOM_SEED = 42

# Model Types
MODEL_TYPES = ["MiniVGG16", "MobileNetV3Small", "SqueezeNet"]

# Optimization - Use librosa MFCC for potentially better accuracy
USE_LIBROSA_MFCC = False  # Set to True for better accuracy with librosa's MFCC