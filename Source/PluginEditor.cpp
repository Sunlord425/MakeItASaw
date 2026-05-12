#include "PluginEditor.h"

static constexpr int kTitleH = 48;
static constexpr int kLabelH = 20;
static constexpr int kPad    = 8;
static constexpr int kHdrH   = 18;  // section header area

SawPluginEditor::SawPluginEditor(SawPluginProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      inputGainAttach (p.apvts, "inputGain",  inputGainKnob),
      toneAttach      (p.apvts, "tone",        toneKnob),
      driveAttach     (p.apvts, "drive",       driveKnob),
      outputGainAttach(p.apvts, "outputGain",  outputGainKnob),
      voicesAttach    (p.apvts, "voices",      voicesKnob),
      detuneAttach    (p.apvts, "detune",      detuneKnob),
      unisonMixAttach (p.apvts, "unisonMix",   unisonMixKnob),
      envFreqAttach   (p.apvts, "envFreq",     envFreqKnob),
      envSensAttach   (p.apvts, "envSens",     envSensKnob),
      envResAttach    (p.apvts, "envRes",      envResKnob)
{
    auto setupKnob = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible(s);
    };
    for (auto* s : { &inputGainKnob, &toneKnob, &driveKnob, &outputGainKnob,
                     &voicesKnob, &detuneKnob, &unisonMixKnob,
                     &envFreqKnob, &envSensKnob, &envResKnob })
        setupKnob(*s);

    auto setupLabel = [this](juce::Label& l, const char* text) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(l);
    };
    setupLabel(inputGainLabel,  "IN GAIN");
    setupLabel(toneLabel,       "TONE");
    setupLabel(driveLabel,      "DRIVE");
    setupLabel(outputGainLabel, "OUT GAIN");
    setupLabel(voicesLabel,     "VOICES");
    setupLabel(detuneLabel,     "DETUNE");
    setupLabel(unisonMixLabel,  "MIX");
    setupLabel(envFreqLabel,    "FREQ");
    setupLabel(envSensLabel,    "SENS");
    setupLabel(envResLabel,     "RES");

    setSize(680, 250);
}

void SawPluginEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a2e));

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(kTitleH - 1, 0.0f, static_cast<float>(getWidth()));

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(26.0f)));
    g.drawText("SAW", 0, 0, getWidth(), kTitleH, juce::Justification::centred);

    auto drawSection = [&](juce::Rectangle<int> b, const char* title) {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(b.toFloat(), 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
        g.drawText(title, b.getX(), b.getY() + 5, b.getWidth(), kHdrH - 5,
                   juce::Justification::centred);
    };

    drawSection(gainBounds,   "GAIN STAGE");
    drawSection(unisonBounds, "UNISON");
    drawSection(envBounds,    "ENV FILTER");
}

void SawPluginEditor::resized() {
    const int secY = kTitleH + kPad;
    const int secH = getHeight() - secY - kPad;

    // Widths proportional to knob count (4 : 3 : 3 = 10 total)
    const int totalW  = getWidth() - 2 * kPad - 2 * kPad;  // outer + inter-section gaps
    const int gainW   = totalW * 4 / 10;
    const int unisonW = totalW * 3 / 10;
    const int envW    = totalW - gainW - unisonW;

    gainBounds   = { kPad,                             secY, gainW,   secH };
    unisonBounds = { kPad + gainW + kPad,               secY, unisonW, secH };
    envBounds    = { kPad + gainW + kPad + unisonW + kPad, secY, envW, secH };

    const int knobY = secY + kHdrH;
    const int knobH = secH - kHdrH - kLabelH - kPad;

    // Place n knobs evenly within a section
    auto place = [&](juce::Rectangle<int> sec,
                     std::initializer_list<std::pair<juce::Slider*, juce::Label*>> items) {
        const int n  = (int)items.size();
        const int kW = sec.getWidth() / n;
        int col = 0;
        for (auto& [knob, label] : items) {
            const int x = sec.getX() + col * kW;
            knob ->setBounds(x, knobY, kW, knobH);
            label->setBounds(x, knobY + knobH, kW, kLabelH);
            ++col;
        }
    };

    place(gainBounds, {
        { &inputGainKnob,  &inputGainLabel  },
        { &toneKnob,       &toneLabel       },
        { &driveKnob,      &driveLabel      },
        { &outputGainKnob, &outputGainLabel },
    });
    place(unisonBounds, {
        { &voicesKnob,    &voicesLabel    },
        { &detuneKnob,    &detuneLabel    },
        { &unisonMixKnob, &unisonMixLabel },
    });
    place(envBounds, {
        { &envFreqKnob, &envFreqLabel },
        { &envSensKnob, &envSensLabel },
        { &envResKnob,  &envResLabel  },
    });
}
