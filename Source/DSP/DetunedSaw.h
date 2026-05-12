#pragma once
#include <cmath>
#include <algorithm>

// Free-running sawtooth oscillator for the unison/detune system.
//
// Derives its frequency from the period estimate provided by SawConverter.
// Detuning in cents shifts the playback rate: positive cents → higher pitch
// (shorter period), negative → lower pitch (longer period).
//
// Because the oscillator is free-running (not reset-locked to the input),
// it drifts in phase relative to the main saw — which is exactly the chorusing
// effect wanted for unison. The period estimate smoothing in SawConverter keeps
// it tracking pitch changes without snapping abruptly.
class DetunedSaw {
public:
    float process(float periodSamples, float detuneCents) noexcept {
        if (periodSamples < 2.0f)
            return 0.0f;

        // Frequency ratio for the detune amount: +100 cents = ×2^(1/12)
        const float freqRatio    = std::exp2(detuneCents / 1200.0f);
        const float detunedPeriod = periodSamples / freqRatio;
        const float increment    = 2.0f / detunedPeriod;   // range is 2 (-1 to +1)

        phase += increment;
        if (phase >= 1.0f)
            phase -= 2.0f;   // wrap: +1 → -1 (sawtooth reset)

        return phase;
    }

    void reset() noexcept { phase = -1.0f; }

private:
    float phase = -1.0f;
};
