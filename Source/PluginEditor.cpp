#include "PluginEditor.h"

static constexpr int kTitleH  = 48;
static constexpr int kLabelH  = 22;
static constexpr int kPad     = 12;
static constexpr int kNumKnobs = 5;

SawPluginEditor::SawPluginEditor(SawPluginProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      inputGainAttach (p.apvts, "inputGain",  inputGainKnob),
      driveAttach     (p.apvts, "drive",       driveKnob),
      detuneAttach    (p.apvts, "detune",      detuneKnob),
      unisonMixAttach (p.apvts, "unisonMix",   unisonMixKnob),
      outputGainAttach(p.apvts, "outputGain",  outputGainKnob)
{
    auto setupKnob = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        addAndMakeVisible(s);
    };
    setupKnob(inputGainKnob);
    setupKnob(driveKnob);
    setupKnob(detuneKnob);
    setupKnob(unisonMixKnob);
    setupKnob(outputGainKnob);

    auto setupLabel = [this](juce::Label& l, const char* text) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(l);
    };
    setupLabel(inputGainLabel,  "IN GAIN");
    setupLabel(driveLabel,      "DRIVE");
    setupLabel(detuneLabel,     "DETUNE");
    setupLabel(unisonMixLabel,  "MIX");
    setupLabel(outputGainLabel, "OUT GAIN");

    setSize(440, 210);
}

void SawPluginEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(kTitleH - 1, 0.0f, static_cast<float>(getWidth()));

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(26.0f)));
    g.drawText("SAW", 0, 0, getWidth(), kTitleH, juce::Justification::centred);
}

void SawPluginEditor::resized() {
    const int colW  = getWidth() / kNumKnobs;
    const int knobY = kTitleH + kPad;
    const int knobH = getHeight() - knobY - kLabelH - kPad;

    auto place = [&](juce::Slider& knob, juce::Label& label, int col) {
        knob .setBounds(col * colW,          knobY,          colW, knobH);
        label.setBounds(col * colW,          knobY + knobH,  colW, kLabelH);
    };

    place(inputGainKnob,  inputGainLabel,  0);
    place(driveKnob,      driveLabel,      1);
    place(detuneKnob,     detuneLabel,     2);
    place(unisonMixKnob,  unisonMixLabel,  3);
    place(outputGainKnob, outputGainLabel, 4);
}
