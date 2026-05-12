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

    // Gain Stage
    juce::Slider inputGainKnob, toneKnob, driveKnob, outputGainKnob;
    juce::Label  inputGainLabel, toneLabel, driveLabel, outputGainLabel;

    // Unison
    juce::Slider voicesKnob, detuneKnob, unisonMixKnob;
    juce::Label  voicesLabel, detuneLabel, unisonMixLabel;

    // Env Filter
    juce::Slider envFreqKnob, envSensKnob, envResKnob;
    juce::Label  envFreqLabel, envSensLabel, envResLabel;

    // Attachments (declared after sliders — destruction order matters)
    juce::AudioProcessorValueTreeState::SliderAttachment
        inputGainAttach, toneAttach, driveAttach, outputGainAttach,
        voicesAttach, detuneAttach, unisonMixAttach,
        envFreqAttach, envSensAttach, envResAttach;

    // Section bounds: set in resized(), read in paint()
    juce::Rectangle<int> gainBounds, unisonBounds, envBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SawPluginEditor)
};
