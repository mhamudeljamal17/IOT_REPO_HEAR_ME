
from TTS.api import TTS
import torch
from TTS.tts.configs.xtts_config import XttsConfig, XttsAudioConfig
from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.models.xtts import XttsArgs
import os

# ==========================================================
# CONFIG
# ==========================================================

# Emotions you want folders for
EMOTIONS = ["happy", "sad", "angry", "neutral"]

SENTENCES = [
    "I will meet you tomorrow.",
    "This is a very interesting idea.",
    "I think we should talk about this.",
    "I don’t know what to do.",
    "Please help me with this problem.",
"Help me!",
    "Please help!",
    "Something is wrong!",
    "I can't breathe!",
    "Stop! Stop!",
    "Get away from me!",
    "I'm scared!",
    "What do I do?!",
    "Everything is under control.",
    "Please open the door now.",
    "I need assistance right now.",
    "Call the police!",
    "Someone is following me!",
    "I can't see anything!",
    "There's smoke everywhere!",
    "I think I'm trapped!",
    "I hear footsteps behind me!",
    "It's getting worse!",
    "I need to get out!",
    "Don't leave me!",
    "I feel dizzy!",
    "The alarm is going off!",
    "Help, someone is hurt!",
    "I can't move!",
    "Please stay with me!",
    "It's too dark!",
    "I hear strange noises!",
    "I can't find the exit!",
    "Everything is shaking!",
    "Is anyone there?!",
"I need your help right now.",
    "Please answer immediately.",
    "This is important, listen carefully."
]

SAMPLES_PER_EMOTION = 10
OUTPUT_ROOT = "dataset"

# Use your own voice recording as reference (voice cloning)
USE_REFERENCE_VOICE = True
REFERENCE_VOICE_WAV = "audio.wav"   # change to your file

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
    kwargs["emotion"]="an"
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

        for i in range(SAMPLES_PER_EMOTION):
            text = SENTENCES[i % len(SENTENCES)]
            out_path = os.path.join(emotion_dir, f"{emotion}_{i:03d}.wav")

            # Currently we are not passing `emotion=` to XTTS;
            # the emotion label is encoded by the folder name.
            synth_one(tts, text, out_path, speaker, language)

            if i % 10 == 0:
                print(f"  [{emotion}] generated {i+1}/{SAMPLES_PER_EMOTION}")

        print(f"Done generating {emotion}.")

    print("\nALL DONE ✅")

if __name__ == "__main__":
    main()