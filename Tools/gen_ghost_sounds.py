# ハント中のループSEを合成して Asset/Sound/SE に書き出す (numpy のみ使用)
#   python Tools/gen_ghost_sounds.py
import os
import wave
import numpy as np

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "Asset", "Sound", "SE")
rng = np.random.default_rng(1234)


def write_wav(name, data):
    data = np.clip(data, -1.0, 1.0)
    pcm = (data * 32767.0).astype(np.int16)
    path = os.path.join(OUT, name)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print("wrote", os.path.normpath(path), f"{len(data) / SR:.2f}s")


def normalize(x, peak=0.9):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def onepole_lp(x, cutoff):
    # 1次ローパス (cutoffは配列でも可 = 時間変化するフィルタ)
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=np.float64), x.shape)
    a = np.exp(-2.0 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1.0 - a[i]) * x[i] + a[i] * s
        y[i] = s
    return y


def bandpass(x, lo, hi):
    return onepole_lp(x, hi) - onepole_lp(x, lo)


def resonator(x, freq, q):
    # 2次の共振フィルタ (声道のフォルマント)。freqは配列でも可 = 口の形が時間で変わる
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), x.shape)
    w = 2.0 * np.pi * freq / SR
    r = np.exp(-w / (2.0 * q))
    a1, a2 = -2.0 * r * np.cos(w), r * r
    g = 1.0 - r
    y = np.zeros_like(x)
    y1 = y2 = 0.0
    for i in range(len(x)):
        v = g[i] * x[i] - a1[i] * y1 - a2[i] * y2
        y2, y1 = y1, v
        y[i] = v
    return y


def smooth_noise(n, rate):
    # rate(Hz)程度でゆらぐ滑らかな乱数(-1~1くらい)
    k = max(2, int(n * rate / SR) + 2)
    pts = rng.uniform(-1, 1, k)
    return np.interp(np.linspace(0, k - 1, n), np.arange(k), pts)


def fade(x, fin=0.01, fout=0.05):
    n_in, n_out = int(fin * SR), int(fout * SR)
    x = x.copy()
    if n_in: x[:n_in] *= np.linspace(0, 1, n_in)
    if n_out: x[-n_out:] *= np.linspace(1, 0, n_out)
    return x


def make_hunt_loop():
    """ハント中に流すループ: 低いうなり + 不協和音 + 心音っぽい鼓動 + かすかなざわめき。継ぎ目なしでループする"""
    dur = 8.0
    n = int(dur * SR)
    t = np.arange(n) / SR
    out = np.zeros(n)
    # ループの継ぎ目を合わせるため、周波数はすべて 1/dur の整数倍にする
    def q(f): return round(f * dur) / dur
    # 低いうなり (うねりのある2音)
    for f, a in [(41.0, 0.5), (41.5, 0.5), (55.0, 0.25), (82.25, 0.12)]:
        out += a * np.sin(2 * np.pi * q(f) * t)
    # 不協和の高音 (短2度でぶつけ、ゆっくり出入りさせる)
    swell = 0.5 - 0.5 * np.cos(2 * np.pi * q(0.25) * t)
    for f, a in [(466.0, 0.05), (494.0, 0.05), (740.0, 0.025)]:
        out += a * swell * np.sin(2 * np.pi * q(f) * t + 0.3 * np.sin(2 * np.pi * q(0.5) * t))
    # 鼓動 (2拍1組、毎秒1組 = 少し速めの心拍)
    beat = np.zeros(n)
    for k in range(int(dur)):
        for off, amp in [(0.0, 1.0), (0.22, 0.7)]:
            s = int((k + off) * SR)
            m = int(0.25 * SR)
            tt = np.arange(m) / SR
            hit = amp * np.sin(2 * np.pi * 50 * tt * (1 - tt * 1.2)) * np.exp(-tt * 22)
            e = min(n, s + m)
            beat[s:e] += hit[: e - s]
    out += beat * 0.9
    # ざわめき (帯域ノイズ、ループに合わせて巡回させる)
    noise = rng.standard_normal(n)
    hiss = bandpass(np.concatenate([noise, noise]), 900, 4000)[n:]  # 2周回してフィルタの立ち上がりを捨てる
    out += hiss * 0.06 * (0.6 + 0.4 * swell)
    write_wav("hunt_loop.wav", normalize(out, 0.8))


if __name__ == "__main__":
    make_hunt_loop()
