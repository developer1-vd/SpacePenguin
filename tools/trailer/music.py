# The trailer's music (docs/assets/trailer.mp4): an original piece, made by this
# script for SpacePenguin and under the same licence. 100 BPM, A minor.
#   python3 tools/trailer/music.py <seconds> music.wav
import numpy as np, wave, sys
SR = 44100; BPM = 100; BEAT = 60 / BPM; BAR = 4 * BEAT
LENGTH = float(sys.argv[1]) if len(sys.argv) > 1 else 36.0
n = int(SR * (LENGTH + 2)); L = np.zeros(n); R = np.zeros(n)
def note(f): return 440 * 2 ** ((f - 69) / 12)
def add(sig, t, pan=0.0, gain=1.0):
    i = int(t * SR); j = min(n, i + len(sig))
    if i >= n: return
    L[i:j] += sig[:j - i] * gain * (1 - pan) / 2 * 2 ** 0.5
    R[i:j] += sig[:j - i] * gain * (1 + pan) / 2 * 2 ** 0.5
def env(length, a, d, s, r):
    k = int(length * SR); e = np.ones(k) * s
    ai, di, ri = int(a * SR), int(d * SR), int(r * SR)
    e[:ai] = np.linspace(0, 1, ai); e[ai:ai + di] = np.linspace(1, s, di)
    e[-ri:] *= np.linspace(1, 0, ri); return e
def pad(midi_notes, length):
    t = np.arange(int(length * SR)) / SR; out = np.zeros_like(t)
    for m in midi_notes:
        for det in (-0.12, 0, 0.12):
            f = note(m + det)
            out += np.sin(2 * np.pi * f * t) + 0.3 * np.sin(4 * np.pi * f * t) + 0.12 * np.sin(6 * np.pi * f * t)
    return out / (len(midi_notes) * 3) * env(length, 0.6, 0.5, 0.8, 0.9)
def pluck(m, length=0.5):
    t = np.arange(int(length * SR)) / SR; f = note(m)
    s = (np.sin(2 * np.pi * f * t) + 0.5 * np.sin(4 * np.pi * f * t) + 0.25 * np.sin(8 * np.pi * f * t))
    return s * np.exp(-t * 7) * env(length, 0.004, 0.05, 1, 0.05)
def kick():
    t = np.arange(int(0.35 * SR)) / SR; f = 110 * np.exp(-t * 18) + 42
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)
def hat():
    t = np.arange(int(0.06 * SR)) / SR; s = np.random.default_rng(1).standard_normal(len(t))
    s = np.diff(s, prepend=0); return s * np.exp(-t * 70) * 0.25
def bass(m, length):
    t = np.arange(int(length * SR)) / SR; f = note(m)
    return (np.sin(2 * np.pi * f * t) + 0.25 * np.sin(4 * np.pi * f * t)) * env(length, 0.01, 0.2, 0.7, 0.15)
# Am - F - C - G, each a bar, as triads around middle C.
chords = [(57, [57, 60, 64]), (53, [57, 60, 65]), (48, [55, 60, 64]), (55, [55, 59, 62])]
arp = [0, 1, 2, 1, 2, 0, 2, 1]
bars = int(LENGTH / BAR) + 1
for b in range(bars):
    root, tri = chords[b % 4]; t0 = b * BAR
    add(pad([x + 12 for x in tri], BAR + 0.8), t0, gain=0.5)
    add(bass(root - 12, BAR * 0.95), t0, gain=0.55)
    if b >= 1:  # the arpeggio enters after the opening bar
        for i, k in enumerate(arp):
            add(pluck(tri[k] + 24, 0.45), t0 + i * BEAT / 2, pan=0.35 if i % 2 else -0.35, gain=0.28)
    if b >= 2:  # the beat after two bars
        for q in range(4):
            add(kick(), t0 + q * BEAT, gain=0.75)
            add(hat(), t0 + q * BEAT + BEAT / 2, pan=0.2, gain=0.6)
# A last chord to ring out on the end card.
end = bars * BAR
add(pad([69, 72, 76, 81], 3.0), min(end, LENGTH - 3.0), gain=0.6)
mix = np.stack([L, R], 1)[: int(LENGTH * SR)]
# Soft echo, then fades and level.
d = int(BEAT * 0.75 * SR); echo = np.zeros_like(mix); echo[d:] = mix[:-d] * 0.25; mix = mix + echo[:, ::-1]
fi, fo = int(1.0 * SR), int(2.5 * SR)
mix[:fi] *= np.linspace(0, 1, fi)[:, None]; mix[-fo:] *= np.linspace(1, 0, fo)[:, None]
mix = mix / np.max(np.abs(mix)) * 0.85
with wave.open(sys.argv[2] if len(sys.argv) > 2 else 'music.wav', 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((mix * 32767).astype('<i2').tobytes())
