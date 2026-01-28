from TTS.api import TTS
import torch
from TTS.tts.configs.xtts_config import XttsConfig, XttsAudioConfig
from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.models.xtts import XttsArgs
import os
import re

# ==========================================================
# CONFIG
# ==========================================================

# Emotions you want folders for
EMOTIONS = ["neutral"]

# Sentences to synthesize
SENTENCES = [
    "System starting. Initializing emotion detection.",
    "Connecting to Wi-Fi...",
    "Wi-Fi connected.",
    "Wi-Fi connection failed. Please check the network.",
    "Syncing system time...",
    "System time synchronized.",
    "Cloud connection established.",
    "Cloud connection failed. System not ready.",
    "Storage error. SD card not detected.",
    "Camera ready.",
    "Camera error. Image capture unavailable.",
    "Listening for emotion.",
    "Recording audio.",
    "Audio recording failed.",
    "Audio saved locally.",
    "Analyzing emotion.",
    "Emotion analysis failed.",
    "No anger detected.",
    "Anger detected.",
    "Optimizing memory...",
    "Capturing image.",
    "Image capture failed.",
    "Uploading data to cloud.",
    "Reconnecting to cloud.",
    "Mentor notified.",
    "Failed to notify mentor.",
    "Image upload failed.",
    "Audio upload failed.",
    "Cloud record creation failed.",
    "Upload completed successfully.",
    "Waiting for response...",
    "New response received.",
    "No response received.",
    "Wi-Fi disconnected.",
    "Low memory warning.",
    "System ready."
]

# How many samples per sentence
SAMPLES_PER_EMOTION = 1

# Output root folder
OUTPUT_ROOT = "dataset"

# Use your own voice recording as reference (voice cloning)
USE_REFERENCE_VOICE = False
REFERENCE_VOICE_WAV = "audio.wav"  # change to your file

# ==========================================================
# Torch allowlist (needed for XTTS)
# ==========================================================

torch.serialization.add_safe_globals([
    XttsConfig,
    XttsAudioConfig,
    BaseDatasetConfig,
    XttsArgs,
])

# ==========================================================
# Helpers
# ==========================================================

def sanitize_filename(text):
    """
    Convert a sentence into a safe filename:
    - Remove special characters like , . ? !
    - Replace spaces with underscores
    - Lowercase all letters
    - Limit to 50 characters
    """
    text = text.lower()
    text = re.sub(r"[^\w\s-]", "", text)  # remove special characters
    text = re.sub(r"\s+", "_", text)      # replace spaces with underscores
    return text[:50]  # limit length

def init_tts():
    print("Starting model load...")
    tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2")
    print("Model loaded successfully.")
    return tts

def pick_speaker_and_language(tts):
    # Speaker
    if not USE_REFERENCE_VOICE and tts.is_multi_speaker:
        speakers_dict = tts.synthesizer.tts_model.speaker_manager.speakers
        speaker = list(speakers_dict.keys())[0]
    else:
        speaker = None

    # Language
    if tts.is_multi_lingual:
        language = tts.synthesizer.tts_model.language_manager.language_names[0]
    else:
        language = None

    print(f"Using speaker={speaker}, language={language}")
    return speaker, language

def synth_one(tts, text, out_path, speaker, language):
    """
    Wraps tts.tts_to_file so we can easily switch between
    reference-voice mode and internal speaker mode.
    """
    kwargs = {
        "text": text,
        "file_path": out_path,
    }

    if USE_REFERENCE_VOICE:
        kwargs["speaker_wav"] = REFERENCE_VOICE_WAV
        if language is not None:
            kwargs["language"] = language
    else:
        if speaker is not None:
            kwargs["speaker"] = speaker
        if language is not None:
            kwargs["language"] = language

    # Optional: set emotion if you want XTTS to encode it
    kwargs["emotion"] = "an"

    tts.tts_to_file(**kwargs)

# ==========================================================
# Main generation logic
# ==========================================================

def main():
    # Init model
    tts = init_tts()
    speaker, language = pick_speaker_and_language(tts)

    # Root folder
    os.makedirs(OUTPUT_ROOT, exist_ok=True)

    for emotion in EMOTIONS:
        emotion_dir = os.path.join(OUTPUT_ROOT, emotion)
        os.makedirs(emotion_dir, exist_ok=True)

        print(f"\n=== Generating emotion: {emotion} ===")

        for text in SENTENCES:
            safe_name = sanitize_filename(text)

            for sample_idx in range(SAMPLES_PER_EMOTION):
                # Add sample index if more than 1 sample per sentence
                if SAMPLES_PER_EMOTION > 1:
                    filename = f"{safe_name}_{sample_idx}.wav"
                else:
                    filename = f"{safe_name}.wav"

                out_path = os.path.join(emotion_dir, filename)

                synth_one(tts, text, out_path, speaker, language)
                print(f"  [{emotion}] generated: {out_path}")

        print(f"Done generating {emotion}.")

    print("\nALL DONE ✅")

if __name__ == "__main__":
    main()
