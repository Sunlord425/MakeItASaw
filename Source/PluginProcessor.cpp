#include "PluginProcessor.h"
#include "PluginEditor.h"

SawPluginProcessor::SawPluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MAKEITASAW_STATE", createLayout())
{}

juce::AudioProcessorValueTreeState::ParameterLayout SawPluginProcessor::createLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputGain", 1}, "Input Gain",
        juce::NormalisableRange<float>(-12.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));

    {
        juce::NormalisableRange<float> toneRange(100.0f, 20000.0f);
        toneRange.setSkewForCentre(1000.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"tone", 1}, "Tone", toneRange, 20000.0f,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
                [](float v, int) -> juce::String {
                    return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz"
                                       : juce::String((int)v) + " Hz";
                })));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drive", 1}, "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"voices", 1}, "Voices", 1, 8, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"detune", 1}, "Detune",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " cts"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"unisonMix", 1}, "Unison Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-20.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));

    {
        juce::NormalisableRange<float> freqRange(200.0f, 8000.0f);
        freqRange.setSkewForCentre(1200.0f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"envFreq", 1}, "Env Freq", freqRange, 4000.0f,
            juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
                [](float v, int) -> juce::String {
                    return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz"
                                       : juce::String((int)v) + " Hz";
                })));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"envSens", 1}, "Env Sens",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"envRes", 1}, "Env Res",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Dry/Wet Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + "%"; })));

    return { params.begin(), params.end() };
}

bool SawPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;
    const auto& set = layouts.getMainInputChannelSet();
    return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
}

void SawPluginProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate;

    const int numChannels = getTotalNumInputChannels();
    const auto n = static_cast<size_t>(numChannels);

    converters  .resize(n);
    shifters    .resize(n * kMaxVoices);
    toneState   .assign(n, 0.0f);
    svfLow      .assign(n, 0.0f);
    svfBand     .assign(n, 0.0f);
    envFollower .assign(n, 0.0f);

    for (auto& c : converters) c.prepare(sampleRate);
    for (auto& s : shifters)   s.reset();

    const float sr_f = static_cast<float>(sampleRate);
    envAtkCoeff = std::exp(-1.0f / (0.003f * sr_f));   // 3 ms attack
    envRelCoeff = std::exp(-1.0f / (0.150f * sr_f));   // 150 ms release

    inputGainParam  = apvts.getRawParameterValue("inputGain");
    toneParam       = apvts.getRawParameterValue("tone");
    driveParam      = apvts.getRawParameterValue("drive");
    voicesParam     = apvts.getRawParameterValue("voices");
    detuneParam     = apvts.getRawParameterValue("detune");
    unisonMixParam  = apvts.getRawParameterValue("unisonMix");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    envFreqParam    = apvts.getRawParameterValue("envFreq");
    envSensParam    = apvts.getRawParameterValue("envSens");
    envResParam     = apvts.getRawParameterValue("envRes");
    dryWetParam     = apvts.getRawParameterValue("dryWet");
}

void SawPluginProcessor::releaseResources() {
    for (auto& c : converters) c.reset();
    for (auto& s : shifters)   s.reset();
    std::fill(svfLow    .begin(), svfLow    .end(), 0.0f);
    std::fill(svfBand   .begin(), svfBand   .end(), 0.0f);
    std::fill(envFollower.begin(), envFollower.end(), 0.0f);
}

void SawPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const float inputGainLin  = juce::Decibels::decibelsToGain(inputGainParam->load());
    const float dryWetMix     = dryWetParam->load() / 100.0f;
    const float toneCutoff    = toneParam->load();
    const float drivePercent  = driveParam->load();
    const int   numExtra      = juce::roundToInt(voicesParam->load()) - 1;
    const float detuneCents   = detuneParam->load();
    const float wet           = unisonMixParam->load() / 100.0f;
    const float outputGainLin = juce::Decibels::decibelsToGain(outputGainParam->load());
    const float envFreqHz     = envFreqParam->load();
    const float envSensPct    = envSensParam->load();
    const float envResPct     = envResParam->load();

    const float sr_f      = static_cast<float>(currentSampleRate);
    const float toneA     = 1.0f - std::exp(-6.2832f * toneCutoff / sr_f);
    const float driveK    = 1.0f + drivePercent * 0.09f;
    const bool  hasDrive  = drivePercent > 0.5f;
    // Also bypass when detune≈0: all ratios would be 1.0, producing delayed copies
    // at the same pitch as the dry signal. Mixing them creates a comb filter whose
    // notches are heard as low-frequency amplitude modulation.
    const bool  hasUnison = numExtra > 0 && wet > 0.001f && detuneCents > 0.01f;
    const bool  hasEnvF   = envSensPct > 0.1f;
    const float normDenom = std::sqrt(1.0f + wet * (float)numExtra);

    float voiceRatios[kMaxVoices];
    if (hasUnison) {
        for (int v = 0; v < numExtra; ++v) {
            const float t     = (numExtra > 1) ? (float)v / (float)(numExtra - 1) : 1.0f;
            const float cents = detuneCents * (2.0f * t - 1.0f);
            voiceRatios[v] = std::exp2(cents / 1200.0f);
        }
    }

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels && ch < static_cast<int>(converters.size()); ++ch) {
        float* data  = buffer.getWritePointer(ch);
        auto&  conv  = converters  [static_cast<size_t>(ch)];
        float& toneZ = toneState   [static_cast<size_t>(ch)];
        float& envF  = envFollower [static_cast<size_t>(ch)];
        float& low   = svfLow      [static_cast<size_t>(ch)];
        float& band  = svfBand     [static_cast<size_t>(ch)];

        // Biquad LP coefficients computed once per block from last block's envelope.
        // Bilinear-transform biquad is stable up to Nyquist; the old Chamberlin SVF
        // became unstable above ~sr/6 (~7.3 kHz at 44.1 kHz).
        float bqB0 = 0.0f, bqB1 = 0.0f, bqB2 = 0.0f, bqA1 = 0.0f, bqA2 = 0.0f;
        if (hasEnvF) {
            const float minFreq = envFreqHz * std::exp2(-envSensPct / 100.0f * 3.0f);
            const float effFreq = juce::jlimit(20.0f, sr_f * 0.49f,
                                               minFreq + envF * (envFreqHz - minFreq));
            const float w0    = 6.2832f * effFreq / sr_f;
            const float sinW0 = std::sin(w0);
            const float cosW0 = std::cos(w0);
            const float Q     = juce::jlimit(0.5f, 10.0f, 0.5f + envResPct / 100.0f * 9.5f);
            const float alpha = sinW0 / (2.0f * Q);
            const float norm  = 1.0f / (1.0f + alpha);
            bqB0 = (1.0f - cosW0) * 0.5f * norm;
            bqB1 = (1.0f - cosW0) * norm;
            bqB2 = bqB0;
            bqA1 = -2.0f * cosW0 * norm;
            bqA2 = (1.0f - alpha) * norm;
        }

        for (int i = 0; i < numSamples; ++i) {
            const float dry = data[i];
            float x = dry * inputGainLin;
            toneZ += toneA * (x - toneZ);
            x = toneZ;
            if (hasDrive) x = std::tanh(driveK * x);

            const float mainSaw = conv.process(x);

            float output = mainSaw;
            if (hasUnison) {
                float voiceSum = 0.0f;
                for (int v = 0; v < numExtra; ++v)
                    voiceSum += shifters[static_cast<size_t>(ch * kMaxVoices + v)].process(mainSaw, voiceRatios[v]);
                output = (mainSaw + wet * voiceSum) / normDenom;
            }

            // Envelope follower on saw output (feeds next block's SVF coefficients).
            const float absOut = std::abs(output);
            const float ec = absOut > envF ? envAtkCoeff : envRelCoeff;
            envF = ec * envF + (1.0f - ec) * absOut;

            // Biquad LP (direct form II transposed). High Q creates the resonant
            // peak that gives the wah character; no separate LP/BP blend needed.
            if (hasEnvF) {
                const float y = bqB0 * output + low;
                low  = bqB1 * output - bqA1 * y + band;
                band = bqB2 * output - bqA2 * y;
                output = y;
            }

            const float wetOut = output * outputGainLin;
            data[i] = dry + dryWetMix * (wetOut - dry);
        }
    }
}

juce::AudioProcessorEditor* SawPluginProcessor::createEditor() {
    return new SawPluginEditor(*this);
}

void SawPluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SawPluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new SawPluginProcessor();
}
