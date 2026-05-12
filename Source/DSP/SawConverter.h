#pragma once
#include <cmath>
#include <algorithm>

// Converts a monophonic pitched signal to a full sawtooth wave.
//
// A rising zero-crossing detector resets the ramp to -1 at the start of each
// fundamental cycle; the ramp rises continuously through the full period.
// The drive stage upstream squares up the signal so each period has exactly
// one dominant rising zero crossing, preventing false triggers from harmonics.
//
// Also tracks the smoothed fundamental period for use by DetunedSaw oscillators.
class SawConverter {
public:
    void prepare(double sampleRate) noexcept {
        sr = static_cast<float>(sampleRate);

        // Ramp covers 2.0 (-1→+1) in one full period at 25 Hz.
        slewRate = 2.0f / (0.040f * sr);

        peakAtk = coeff(0.0001f);
        peakRel = coeff(0.3f);
        envAtk  = coeff(0.0001f);  // 0.1 ms — fast enough to be imperceptible at onset
        envRel  = coeff(0.08f);

        reset();
    }

    void reset() noexcept {
        slewPrev           = -1.0f;
        armed              = true;   // ready to fire on the very first rising edge
        peakEnv            = 0.0f;
        dcX1 = dcY1        = 0.0f;
        inputEnv           = 0.0f;
        recentInputPeak    = 0.0f;
        samplesSinceCross  = 0;
        smoothedPeriod     = 0.0f;
        periodValid        = false;
    }

    float process(float x) noexcept {
        // Period tracking — count samples since last rising crossing.
        // After 50 ms without a crossing (silence / very low freq), invalidate the
        // period and re-arm so the next note's first rising edge fires immediately
        // rather than waiting for a prior negative half-cycle.
        ++samplesSinceCross;
        if (samplesSinceCross > static_cast<int>(sr * 0.05f)) {
            periodValid = false;
            armed       = true;
        }

        // 1. Rising zero-crossing detector with armed hysteresis.
        // Threshold of -0.01 prevents tiny harmonic dips from arming the trigger.
        if (x < -0.01f)
            armed = true;

        if (armed && x >= 0.0f) {
            // When period is known, require at least half a period between resets so
            // harmonics within the same cycle cannot cause spurious re-triggers.
            const int minGap = periodValid
                ? std::max(10, static_cast<int>(smoothedPeriod * 0.5f))
                : 10;
            if (samplesSinceCross >= minGap) {
                const float raw = static_cast<float>(samplesSinceCross);
                smoothedPeriod = periodValid ? 0.9f * smoothedPeriod + 0.1f * raw : raw;
                periodValid    = true;
            }
            samplesSinceCross = 0;
            armed    = false;
            slewPrev = -1.0f;

            // Pre-charge inputEnv to last cycle's peak so note onsets are
            // instantaneous rather than fading in from 0.
            inputEnv        = recentInputPeak;
            recentInputPeak = 0.0f;

            // Pre-charge peakEnv to the expected ramp amplitude for this pitch.
            // Without this, a note played after a lower-frequency note inherits a
            // peakEnv that's too high, producing LF amplitude drift until peakEnv
            // finally decays to the correct level (which can take seconds).
            if (periodValid)
                peakEnv = std::min(slewRate * smoothedPeriod, 2.0f);
        } else {
            slewPrev = std::min(slewPrev + slewRate, 1.0f);
        }

        recentInputPeak = std::max(recentInputPeak, std::abs(x));

        // 2. Amplitude normalization via peak follower.
        const float shifted = slewPrev + 1.0f;   // [0, 2] at low freq, smaller at high
        if (shifted > peakEnv)
            peakEnv = peakAtk * peakEnv + (1.0f - peakAtk) * shifted;
        else
            peakEnv *= peakRel;

        float norm = 0.0f;
        if (peakEnv > 0.005f)
            norm = 2.0f * shifted / peakEnv - 1.0f;

        // 3. DC block (safety — full sawtooth has near-zero DC)
        const float dc = norm - dcX1 + dcCoeff * dcY1;
        dcX1 = norm;
        dcY1 = dc;

        // 4. Input envelope follower
        const float ax = std::abs(x);
        const float ec = ax > inputEnv ? envAtk : envRel;
        inputEnv = ec * inputEnv + (1.0f - ec) * ax;

        return dc * inputEnv;
    }

    // Period of the detected fundamental in samples (0 if not yet valid).
    float getSmoothedPeriod()  const noexcept { return periodValid ? smoothedPeriod : 0.0f; }
    float getInputEnvelope()   const noexcept { return inputEnv; }

private:
    float sr       = 44100.0f;
    float slewRate = 0.0f;
    float slewPrev = -1.0f;
    bool  armed    = false;

    int   samplesSinceCross = 0;
    float smoothedPeriod    = 0.0f;
    bool  periodValid       = false;

    float peakEnv = 0.0f;
    float peakAtk = 0.0f, peakRel = 0.0f;

    float dcX1 = 0.0f, dcY1 = 0.0f;
    static constexpr float dcCoeff = 0.9995f;

    float inputEnv      = 0.0f;
    float recentInputPeak = 0.0f;
    float envAtk        = 0.0f, envRel = 0.0f;

    float coeff(float t) const noexcept {
        return std::exp(-1.0f / (t * sr));
    }
};
