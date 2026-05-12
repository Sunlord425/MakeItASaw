#include "PluginProcessor.h"
#include "PluginEditor.h"

SawPluginProcessor::SawPluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "SAW_STATE", createLayout())
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

    converters.resize(n);
    shifters  .resize(n * kMaxVoices);
    toneState .assign(n, 0.0f);

    for (auto& c : converters) c.prepare(sampleRate);
    for (auto& s : shifters)   s.reset();

    inputGainParam  = apvts.getRawParameterValue("inputGain");
    toneParam       = apvts.getRawParameterValue("tone");
    driveParam      = apvts.getRawParameterValue("drive");
    voicesParam     = apvts.getRawParameterValue("voices");
    detuneParam     = apvts.getRawParameterValue("detune");
    unisonMixParam  = apvts.getRawParameterValue("unisonMix");
    outputGainParam = apvts.getRawParameterValue("outputGain");
}

void SawPluginProcessor::releaseResources() {
    for (auto& c : converters) c.reset();
    for (auto& s : shifters)   s.reset();
}

void SawPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const float inputGainLin  = juce::Decibels::decibelsToGain(inputGainParam->load());
    const float toneCutoff    = toneParam->load();
    const float drivePercent  = driveParam->load();
    const int   numExtra      = juce::roundToInt(voicesParam->load()) - 1;
    const float detuneCents   = detuneParam->load();
    const float wet           = unisonMixParam->load() / 100.0f;
    const float outputGainLin = juce::Decibels::decibelsToGain(outputGainParam->load());

    // 1-pole LP coefficient: a = 1 - exp(-2π·fc/sr). At fc=20kHz → nearly all-pass.
    const float toneA  = 1.0f - std::exp(-6.2832f * toneCutoff / (float)currentSampleRate);
    const float driveK = 1.0f + drivePercent * 0.09f;
    const bool  hasDrive  = drivePercent > 0.5f;
    const bool  hasUnison = numExtra > 0 && wet > 0.001f;

    // Equal-power normalization denominator: sqrt(1 + wet·N) keeps perceived loudness
    // stable as voice count increases despite partial phase cancellation between voices.
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
        auto&  conv  = converters[static_cast<size_t>(ch)];
        float& toneZ = toneState [static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            float x = data[i] * inputGainLin;

            // Pre-filter: 1-pole LP before drive to suppress harmonics entering the ZCD.
            toneZ += toneA * (x - toneZ);
            x = toneZ;

            if (hasDrive)
                x = std::tanh(driveK * x);

            const float mainSaw = conv.process(x);

            float output = mainSaw;
            if (hasUnison) {
                float voiceSum = 0.0f;
                for (int v = 0; v < numExtra; ++v)
                    voiceSum += shifters[static_cast<size_t>(ch * kMaxVoices + v)].process(mainSaw, voiceRatios[v]);

                output = (mainSaw + wet * voiceSum) / normDenom;
            }

            data[i] = output * outputGainLin;
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
