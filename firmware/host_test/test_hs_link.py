"""Confere o contrato da revisão B sem o compilador C."""

INDEX = [-1, -1, -1, -1, 2, 4, 6, 8] * 2
STEP = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
]


def clamp16(value):
    return max(-32768, min(32767, value))


class State:
    def __init__(self):
        self.predictor = 0
        self.index = 0


def decode_nibble(state, nibble):
    index = max(0, min(88, state.index))
    step = STEP[index]
    delta = step >> 3
    if nibble & 4:
        delta += step
    if nibble & 2:
        delta += step >> 1
    if nibble & 1:
        delta += step >> 2
    predictor = state.predictor - delta if nibble & 8 else state.predictor + delta
    state.predictor = clamp16(predictor)
    state.index = max(0, min(88, index + INDEX[nibble & 0x0F]))
    return state.predictor


def encode_sample(state, sample):
    index = max(0, min(88, state.index))
    step = STEP[index]
    diff = sample - state.predictor
    nibble = 0
    if diff < 0:
        nibble = 8
        diff = -diff
    delta = step >> 3
    if diff >= step:
        nibble |= 4
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        nibble |= 2
        diff -= step
        delta += step
    step >>= 1
    if diff >= step:
        nibble |= 1
        delta += step
    decode_nibble(state, nibble)
    return nibble


def one_way(frame, jitter):
    return frame + 1 + jitter * frame + 11


def battery(mv):
    knots = [3300, 3400, 3550, 3700, 3800, 3900, 4000, 4150]
    pct = [0, 8, 20, 40, 55, 70, 85, 100]
    if mv <= knots[0]:
        return 0
    if mv >= knots[-1]:
        return 100
    for i in range(7):
        if mv <= knots[i + 1]:
            span = knots[i + 1] - knots[i]
            rise = pct[i + 1] - pct[i]
            return pct[i] + (mv - knots[i]) * rise // span
    return 100


def main():
    assert len(STEP) == 89
    assert one_way(5, 2) == 27
    assert 2 * one_way(5, 2) + 8 == 62
    assert one_way(5, 1) == 22
    assert (10 // 5) * 2 == 4
    assert battery(4200) == 100
    assert battery(3200) == 0
    assert battery(4150) == 100
    assert battery(3300) == 0

    import math
    samples = [int(12000 * math.sin(2 * math.pi * 1000 * i / 48000)) for i in range(240)]
    enc = State()
    dec = State()
    out = []
    for sample in samples:
        out.append(decode_nibble(dec, encode_sample(enc, sample)))
    err = sum((a - b) ** 2 for a, b in zip(samples, out)) / len(samples)
    rms = math.sqrt(err)
    assert rms < 1500, rms

    groups = [3, 3, 3, 9, 0, 0]
    decimated = [(groups[i] + groups[i + 1] + groups[i + 2]) // 3 for i in (0, 3)]
    assert decimated == [3, 3]
    up = []
    src = [0, 300]
    for i, a in enumerate(src):
        b = src[i + 1] if i + 1 < len(src) else a
        up.extend([a, (2 * a + b) // 3, (a + 2 * b) // 3])
    assert up[0] == 0 and up[3] == 300
    print("ok", round(rms, 1))


if __name__ == "__main__":
    main()
