"""
Audio Processing Module - MFCC Feature Extraction
Updated to use librosa for better accuracy (as shown in notebooks)
"""

import numpy as np
from scipy.fft import dct
import librosa
from config import (
    SAMPLING_RATE, DURATION, NUM_SAMPLES, SIZE_WIN, SIZE_OFF,
    TOTAL_WIN_NUM, FREQ_MIN, FREQ_MAX, MEL_BANDS, USE_LIBROSA_MFCC
)


def load_and_process_audio_librosa(file_path):
    """
    Load audio and extract MFCC features using librosa (RECOMMENDED)
    This matches the notebook implementation and provides better accuracy
    """
    try:
        # Load audio
        audio, sr = librosa.load(file_path, sr=SAMPLING_RATE, mono=True, duration=DURATION)

        # Calculate spectrogram for energy-based voice activity detection
        spectrogram = np.abs(librosa.stft(audio, n_fft=512, hop_length=256))
        spectrogram_db = librosa.amplitude_to_db(spectrogram, ref=np.max)

        # Find energy transitions to detect speech onset
        max_energy = np.mean(spectrogram_db, axis=0)
        start_pos = 0

        for i in range(1, max_energy.shape[0]):
            diff_energy = abs(max_energy[i] - max_energy[i - 1])
            if diff_energy >= 5:
                col = spectrogram_db.shape[1]
                mues_x_col = int(NUM_SAMPLES / col)
                start_pos = i * mues_x_col
                break

        # Reload audio with extended duration
        audio_extended, _ = librosa.load(file_path, sr=SAMPLING_RATE, mono=True, duration=DURATION + 2)
        audio = audio_extended[start_pos:start_pos + NUM_SAMPLES]

        # Pad or truncate to correct length
        if audio.shape[0] < NUM_SAMPLES:
            audio = np.append(audio, np.zeros(NUM_SAMPLES - audio.shape[0]))
        else:
            audio = audio[:NUM_SAMPLES]

        # Add white noise augmentation (as in notebooks)
        mean = np.mean(audio)
        sd = np.std(audio)
        white_noise = np.random.normal(loc=mean, scale=abs(sd / 50), size=NUM_SAMPLES)
        audio_augmented = audio + white_noise

        # Compute MFCC using librosa (matches notebook exactly)
        mfcc_features = librosa.feature.mfcc(
            y=audio_augmented,
            sr=SAMPLING_RATE,
            n_mfcc=MEL_BANDS,
            n_fft=SIZE_WIN,
            hop_length=SIZE_OFF,
            n_mels=MEL_BANDS
        )

        return mfcc_features

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return None


def triangular_fun(lower_lim, medium_lim, upper_lim, points_num):
    """Calculate triangular filter coefficients"""
    coefficients = np.zeros(points_num)
    for j in range(points_num):
        if j >= lower_lim and j <= medium_lim:
            coefficients[j] = (j - lower_lim) / (medium_lim - lower_lim)
        elif j > medium_lim and j <= upper_lim:
            coefficients[j] = (upper_lim - j) / (upper_lim - medium_lim)
        else:
            coefficients[j] = 0
    return coefficients


def triangular_filter_mat(frec_min, frec_max, mel_bands):
    """Create Mel-scale triangular filter bank"""
    mel_num_limits = mel_bands + 2
    frec_min_mel = 2595 * np.log10(1 + frec_min / 700)
    frec_max_mel = 2595 * np.log10(1 + frec_max / 700)
    separation_mel = (frec_max_mel - frec_min_mel) / (mel_num_limits - 1)

    mel_limits = np.zeros(mel_num_limits)
    mel_limits[0] = frec_min_mel
    for i in range(1, mel_num_limits):
        mel_limits[i] = mel_limits[i - 1] + separation_mel

    Hz_limits = np.zeros(mel_num_limits)
    for i in range(mel_num_limits):
        Hz_limits[i] = 700 * (10 ** (mel_limits[i] / 2595) - 1)

    FFT_bins = np.zeros(mel_num_limits)
    for i in range(mel_num_limits):
        FFT_bins[i] = int((SIZE_WIN // 2 + 1) * Hz_limits[i] / frec_max)
    FFT_bins[mel_num_limits - 1] = SIZE_WIN // 2

    Trian_mat = np.zeros((mel_bands, SIZE_WIN // 2 + 1))
    for i in range(2, mel_num_limits):
        trian_filter = triangular_fun(int(FFT_bins[i - 2]), int(FFT_bins[i - 1]),
                                      int(FFT_bins[i]), SIZE_WIN // 2 + 1)
        Trian_mat[i - 2, :] = trian_filter

    return Trian_mat


def preemphasis(signal, coeff=0.97):
    """Apply pre-emphasis filter"""
    return np.append(signal[0], signal[1:] - coeff * signal[:-1])


def windowing(hamming_window, preemphasized_signal):
    """Apply Hamming windowing"""
    win_mat = np.zeros((SIZE_WIN, TOTAL_WIN_NUM))
    cont = 0
    for i in range(TOTAL_WIN_NUM):
        for j in range(SIZE_WIN):
            win_mat[j][i] = hamming_window[j] * preemphasized_signal[cont]
            cont = cont + 1
        cont = cont - (SIZE_WIN - SIZE_OFF)
    return win_mat


def fft_mat(win_mat):
    """Compute FFT for each window"""
    fft_mat_result = np.zeros((1 + SIZE_WIN // 2, TOTAL_WIN_NUM))
    for i in range(TOTAL_WIN_NUM):
        fft = abs(np.fft.fft(win_mat[:, i], n=SIZE_WIN))
        fft_mat_result[:, i] = fft[0:1 + SIZE_WIN // 2]
    return fft_mat_result


def compute_mfcc_custom(audio):
    """Compute MFCC using custom implementation (matches notebook)"""
    # Create Hamming window
    hamming_window = np.zeros(SIZE_WIN)
    for i in range(SIZE_WIN):
        hamming_window[i] = 0.53836 - 0.46164 * np.cos((2 * np.pi * i) / (SIZE_WIN - 1))

    # Calculate triangular filter bank
    trian_mat = triangular_filter_mat(FREQ_MIN, FREQ_MAX, MEL_BANDS)

    # Pre-emphasis
    preemphasized_signal = preemphasis(audio)

    # Windowing
    win_mat = windowing(hamming_window, preemphasized_signal)

    # FFT
    fft_matrix = fft_mat(win_mat)

    # Apply Mel filter bank
    mel_mat = np.dot(trian_mat, fft_matrix)

    # Log transformation
    mel_mat_log = 13 * np.log(mel_mat + 1e-10)

    # DCT
    mfcc = dct(mel_mat_log, type=2, axis=0, norm='ortho')[:MEL_BANDS]

    return mfcc


def compute_mfcc_batch(audios, use_librosa=USE_LIBROSA_MFCC):
    """Compute MFCCs for a batch of audio signals"""
    mfccs = []
    for i, audio in enumerate(audios):
        if use_librosa:
            mfcc = librosa.feature.mfcc(
                y=audio,
                sr=SAMPLING_RATE,
                n_mfcc=MEL_BANDS,
                n_fft=SIZE_WIN,
                hop_length=SIZE_OFF,
                n_mels=MEL_BANDS
            )
        else:
            mfcc = compute_mfcc_custom(audio)

        mfccs.append(mfcc)
        if (i + 1) % 50 == 0:
            print(f"  Processed {i + 1}/{len(audios)} audios")

    return np.asarray(mfccs)