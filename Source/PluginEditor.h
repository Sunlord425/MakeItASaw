#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SawPluginEditor : public juce::AudioProcessorEditor {
public:
    explicit SawPluginEditor(SawPluginProcessor&);
    ~SawPluginEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SawPluginProcessor& proc;

    juce::Slider inputGainKnob, driveKnob, voicesKnob, detuneKnob, unisonMixKnob, outputGainKnob;
    juce::Label  inputGainLabel, driveLabel, voicesLabel, detuneLabel, unisonMixLabel, outputGainLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment
        inputGainAttach, driveAttach, voicesAttach, detuneAttach, unisonMixAttach, outputGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SawPluginEditor)
};
