#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ScopeView.h"

class LowLinkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LowLinkLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
};

class LowLinkEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit LowLinkEditor (LowLinkProcessor&);
    ~LowLinkEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshLinkList();
    void acquire();
    void prepareTrack (const float* raw, int len, int dec, double sr, double lpHz, std::vector<float>& out);
    void syncChannelButtons();
    void setChannel (int index);
    ScopeView::Settings currentSettings() const;
    int choiceIndex (const char* id) const;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    LowLinkProcessor& proc;
    LowLinkLookAndFeel lnf;

    ScopeView scope;

    juce::Label titleLabel, nameLabel, linkLabel, sizeLabel, showLabel, lpLabel;
    juce::Label ampLabel, selfLevelLabel, linkLevelLabel, zoomLabel, posLabel;
    juce::TextEditor nameEditor;
    juce::ComboBox linkBox, sizeBox, lpBox;
    juce::TextButton chanButtons[3];
    juce::TextButton mixButton { "Sum" }, freezeButton { "Freeze" }, liveButton { "Live" };
    juce::Slider ampSlider, selfLevelSlider, linkLevelSlider, zoomSlider, posSlider;

    std::unique_ptr<ComboAttachment> sizeAtt, lpAtt;
    std::unique_ptr<ButtonAttachment> mixAtt, liveAtt;
    std::unique_ptr<SliderAttachment> ampAtt, selfAtt, linkAtt, zoomAtt, posAtt;

    juce::Rectangle<int> statsArea;

    // acquisition state
    struct Key
    {
        int64_t start = -1; int head = -1; double size = 0; int lp = -1; int target = -2;
        bool timeline = false, live = true;
        bool operator== (const Key& o) const
        {
            return start == o.start && head == o.head && juce::exactlyEqual (size, o.size) && lp == o.lp
                && target == o.target && timeline == o.timeline && live == o.live;
        }
    } lastKey;
    int64_t lastFreeRunCapture = -1;
    int listCounter = 0;
    std::vector<lowlink::SlotInfo> linkCandidates;
    std::vector<float> tmpA, tmpB, rawA, rawB;
    lowlink::Analysis cachedAnalysis;
    int64_t analysedStart = -1;
    int analysedTarget = -2, analysedLp = -1;
    double analysedSize = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowLinkEditor)
};
