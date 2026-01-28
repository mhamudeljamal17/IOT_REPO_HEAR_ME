import argparse
from TTS.api import TTS
import torch
from TTS.tts.configs.xtts_config import XttsConfig, XttsAudioConfig
from TTS.config.shared_configs import BaseDatasetConfig
from TTS.tts.models.xtts import XttsArgs
from pydub import AudioSegment
import os

# -------------------------------
# Parse command-line arguments
# -------------------------------
parser = argparse.ArgumentParser(description="Generate neutral and emergency TTS voices.")
parser.add_argument(
    "param",
    nargs="?",
    default=None,
    help="Either a speaker ID (integer) or a path to a .wav file for speaker_wav"
)
args = parser.parse_args()

param = args.param
speaker_id = -8  # default
speaker_wav = None

# Determine if param is number or wav
if param:
    if param.lower().endswith(".wav") and os.path.exists(param):
        speaker_wav = param
        print(f"Using speaker WAV: {speaker_wav}")
    else:
        try:
            speaker_id = int(param)
            print(f"Using speaker ID: {speaker_id}")
        except ValueError:
            print(f"Invalid parameter '{param}', using default speaker ID: {speaker_id}")

# -------------------------------
# Allowlist classes
# -------------------------------
torch.serialization.add_safe_globals([
    XttsConfig,
    XttsAudioConfig,
    BaseDatasetConfig,
    XttsArgs
])

# -------------------------------
# Load TTS model
# -------------------------------
print("Loading XTTS...")
tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2")
print("Model loaded.")

# -------------------------------
# Select speaker
# -------------------------------
if tts.is_multi_speaker:
    speakers_dict = tts.synthesizer.tts_model.speaker_manager.speakers
    speaker_list = list(speakers_dict.keys())
    if speaker_wav is None:
        # Only use speaker_id if no WAV
        if speaker_id < 0:
            speaker = speaker_list[speaker_id]
        else:
            speaker = speaker_list[speaker_id % len(speaker_list)]
        print(f"Selected speaker: {speaker}")
    else:
        speaker = None
else:
    speaker = None

# -------------------------------
# Select language
# -------------------------------
if tts.is_multi_lingual:
    language = tts.synthesizer.tts_model.language_manager.language_names[0]
else:
    language = None

# -------------------------------
# Determine folder prefix
# -------------------------------
if speaker_wav:
    prefix = os.path.splitext(os.path.basename(speaker_wav))[0][:3]  # first 3 letters
elif speaker is not None:
    prefix = str(speaker_id)
else:
    prefix = "default"

# -------------------------------
# Sentences for TTS
# -------------------------------
sentences = [
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

# -------------------------------
# Create dataset folders
# -------------------------------
neutral_folder = f"dataset/tts/{prefix}/neutral"
emergency_folder = f"dataset/tts/{prefix}/emergency"
os.makedirs(neutral_folder, exist_ok=True)
os.makedirs(emergency_folder, exist_ok=True)

# -------------------------------
# Function to generate emergency voice
# -------------------------------
def make_emergency_voice(path, level="low"):
    audio = AudioSegment.from_wav(path)

    if level == "low":
        speed_factor = 1.05
        pitch_factor = 1.05
        gain_db = 2
    elif level == "medium":
        speed_factor = 1.15
        pitch_factor = 1.1
        gain_db = 4
    elif level == "high":
        speed_factor = 1.25
        pitch_factor = 1.15
        gain_db = 6
        audio = audio.compress_dynamic_range(threshold=-30)
    else:
        raise ValueError("level must be 'low', 'medium' or 'high'")

    audio = audio.speedup(speed_factor)
    audio = audio._spawn(audio.raw_data, overrides={
        "frame_rate": int(audio.frame_rate * pitch_factor)
    }).set_frame_rate(audio.frame_rate)
    audio = audio + gain_db

    audio.export(path, format="wav")

# -------------------------------
# Generate TTS
# -------------------------------
for i in range(len(sentences)):
    text = sentences[i % len(sentences)]

    # Neutral
    out_neutral = f"{neutral_folder}/neutral_{i}.wav"
    tts.tts_to_file(
        text=text,
        file_path=out_neutral,
        speaker=speaker,
        speaker_wav=speaker_wav,
        language=language
    )
    print("Generated Neutral:", out_neutral)

    # Panic / Emergency levels
    for level in ["low", "medium", "high"]:
        out_emergency = f"{emergency_folder}/emergency_{level}_{i}.wav"
        tts.tts_to_file(
            text=text,
            file_path=out_emergency,
            speaker=speaker,
            speaker_wav=speaker_wav,
            language=language
        )

        make_emergency_voice(out_emergency, level)
        print(f"Generated {level.upper()} EMERGENCY:", out_emergency)

print("DONE 🎉")
