import os
import random
import soundfile as sf
from voicebox import Voicebox

# =======================
# CONFIG
# =======================
OUTPUT_DIR = "voicebox_dataset"
N_SAMPLES = 40  # how many per emotion

# Emotions we want
EMOTIONS = ["neutral", "happy", "sad", "angry", "fear"]

# Example sentences
SENTENCES = [
    "I am here.",
    "Please open the door.",
    "This is a simple message.",
    "Everything is going fine.",
    "I need help right now.",
    "Can someone hear me?",
    "This is very important.",
    "Please respond quickly.",
    "I am scared.",
    "I feel something terrible is happening."
]

# =======================
# MAKE FOLDERS
# =======================
for emo in EMOTIONS:
    os.makedirs(os.path.join(OUTPUT_DIR, emo), exist_ok=True)

# =======================
# LOAD VOICEBOX TTS
# =======================
vb = Voicebox(model="voicebox-base-en")  # English base model

# =======================
# GENERATE
# =======================
for emo in EMOTIONS:
    print(f"[INFO] Generating for emotion: {emo}")

    for i in range(N_SAMPLES):
        text = random.choice(SENTENCES)

        # optional text tweak to push expressiveness
        if emo in ["happy", "angry", "fear"]:
            text = text + "!"

        # synthesize
        wav = vb.text2speech(text, emotion=emo)

        # save
        path = os.path.join(OUTPUT_DIR, emo, f"{emo}_{i}.wav")
        sf.write(path, wav, vb.sample_rate)

        if (i+1) % 10 == 0:
            print(f"   {i+1}/{N_SAMPLES} done")

print("\n✅ Finished generating VoiceBox emotional dataset!")
