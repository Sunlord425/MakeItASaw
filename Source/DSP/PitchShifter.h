#pragma once
#include <cmath>
#include <algorithm>

// Delay-based pitch shifter using two alternating read heads with cross-fading.
// Works on any audio signal — no pitch detection required.
//
// Two read heads traverse a ring buffer at `ratio` samples/sample (the write head
// always advances at 1 sample/sample). When a head drifts too close to the write
// position or to the far end of the buffer, a cross-fade transfers to a fresh head
// placed back at the centre of the buffer.
//
// kBufLen/2 samples of latency (~5.8 ms at 44.1 kHz) is inherent and deliberate:
// the phase offset between the shifted copies and the main signal produces the
// chorusing spread that makes unison sound wide rather than thin.
class PitchShifter {
public:
    static constexpr int kBufLen   = 512;
    static constexpr int kMask     = kBufLen - 1;
    static constexpr int kXfadeLen = 128;   // ~2.9 ms at 44.1 kHz — longer window reduces glitches on high notes

    void reset() noexcept {
        std::fill(buf, buf + kBufLen, 0.0f);
        writeIdx   = 0;
        readA      = static_cast<float>(kBufLen) / 2.0f;
        readB      = 0.0f;
        xfadeCount = -1;
    }

    // ratio = 2^(detuneCents/1200). Compute once per block, not per sample.
    float process(float input, float ratio) noexcept {
        buf[writeIdx & kMask] = input;

        // Delay of primary head behind the write head
        const float delayA = std::fmod(static_cast<float>(writeIdx) - readA + kBufLen, static_cast<float>(kBufLen));

        // Trigger cross-fade when primary head drifts too close to either boundary
        if (xfadeCount < 0 && (delayA < kXfadeLen || delayA > kBufLen - kXfadeLen)) {
            readB = static_cast<float>(writeIdx) - static_cast<float>(kBufLen) / 2.0f;
            if (readB < 0.0f) readB += kBufLen;
            xfadeCount = 0;
        }

        float out;
        if (xfadeCount >= 0) {
            const float t     = static_cast<float>(xfadeCount) / static_cast<float>(kXfadeLen);
            const float tCos  = 0.5f - 0.5f * std::cos(t * 3.14159265f);
            out = lerp(readA) * (1.0f - tCos) + lerp(readB) * tCos;

            readB += ratio;
            if (readB >= kBufLen) readB -= kBufLen;

            if (++xfadeCount >= kXfadeLen) {
                readA      = readB;
                xfadeCount = -1;
            }
        } else {
            out = lerp(readA);
        }

        readA += ratio;
        if (readA >= kBufLen) readA -= kBufLen;

        // Keep writeIdx bounded to [0, kBufLen) so float(writeIdx) never loses
        // integer precision. Without this, after ~6 min the delay calculation
        // silently drifts and read heads jump to wrong positions.
        writeIdx = (writeIdx + 1) & kMask;
        return out;
    }

private:
    float buf[kBufLen] = {};
    int   writeIdx = 0;
    float readA    = 0.0f;
    float readB    = 0.0f;
    int   xfadeCount = -1;

    float lerp(float p) const noexcept {
        const int   i0 = static_cast<int>(p) & kMask;
        const int   i1 = (i0 + 1) & kMask;
        const float f  = p - static_cast<float>(static_cast<int>(p));
        return buf[i0] + f * (buf[i1] - buf[i0]);
    }
};
