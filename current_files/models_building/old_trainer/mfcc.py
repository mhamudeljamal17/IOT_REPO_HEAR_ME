
# this is not dibrose, as they stated that a custom implementation was developed to ensure consistency between Python and C++

import numpy as np
from scipy.fftpack import dct

def pre_emphasis(signal, alpha=0.97):
    return np.append(signal[0], signal[1:] - alpha * signal[:-1])

def frame_signal(signal, frame_len=256, hop=128):
    frames = []
    for i in range(0, len(signal) - frame_len, hop):
        frames.append(signal[i:i+frame_len])
    return np.array(frames)

def hamming_window(N):
    return 0.53836 - 0.46164 * np.cos(2 * np.pi * np.arange(N) / (N - 1))

def mel_filterbank(n_mels, n_fft, fs):
    mel_min = 2595 * np.log10(1 + 20 / 700)
    mel_max = 2595 * np.log10(1 + (fs / 2) / 700)
    mel_points = np.linspace(mel_min, mel_max, n_mels + 2)
    hz_points = 700 * (10**(mel_points / 2595) - 1)

    bins = np.floor((n_fft + 1) * hz_points / fs).astype(int)
    fb = np.zeros((n_mels, n_fft // 2 + 1))

    for m in range(1, n_mels + 1):
        f_m_minus = bins[m - 1]
        f_m = bins[m]
        f_m_plus = bins[m + 1]

        for k in range(f_m_minus, f_m):
            fb[m - 1, k] = (k - f_m_minus) / (f_m - f_m_minus)
        for k in range(f_m, f_m_plus):
            fb[m - 1, k] = (f_m_plus - k) / (f_m_plus - f_m)

    return fb

def compute_mfcc(signal, fs=4096, n_mels=20):
    signal = pre_emphasis(signal)
    frames = frame_signal(signal)
    frames *= hamming_window(256)

    fft_power = np.abs(np.fft.rfft(frames, 256))**2
    mel_fb = mel_filterbank(n_mels, 256, fs)
    mel_energy = np.dot(fft_power, mel_fb.T)
    log_mel = np.log(mel_energy + 1e-9)

    mfcc = dct(log_mel, type=2, axis=1, norm='ortho')
    return mfcc[:, :n_mels].T
