#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Analysis.h"

namespace Colours_
{
    const juce::Colour bg        { 0xff111317 };
    const juce::Colour panel     { 0xff1a1d22 };
    const juce::Colour panelEdge { 0xff2a2e35 };
    const juce::Colour grid      { 0xff23272e };
    const juce::Colour text      { 0xffd7dbe0 };
    const juce::Colour dim       { 0xff7d848e };
    const juce::Colour self      { 0xffff7a45 }; // this track
    const juce::Colour link      { 0xff3ddbd9 }; // linked track
    const juce::Colour sum       { 0xffeef1f5 };
    const juce::Colour conflict  { 0xffff3b4e };
    const juce::Colour good      { 0xff5ad17a };
}

struct ScopeFrame
{
    std::vector<float> a; // this track
    std::vector<float> b; // linked track
    bool hasA = false, hasB = false;
    double sampleRate = 44100.0;
    double sizeBeats = 1.0;
    juce::String nameA, nameB;
    juce::String status;          // e.g. "Transport stopped - approximate"
    float headFrac = -1.0f;       // live sweep: write head position 0..1 (-1 = none)
    bool lowpassed = true;        // band-limited data can be drawn as a smooth curve
    lowlink::Analysis analysis;
};

class ScopeView : public juce::Component
{
public:
    struct Settings
    {
        int channel = 1;       // 0 this, 1 both, 2 linked
        bool mix = false;
        float amp = 2.0f;
        float selfLevel = 1.0f, linkLevel = 1.0f;
        float zoom = 1.0f, position = 0.0f;
        bool frozen = false;

        bool operator!= (const Settings& o) const
        {
            return channel != o.channel || mix != o.mix || amp != o.amp || selfLevel != o.selfLevel
                || linkLevel != o.linkLevel || zoom != o.zoom || position != o.position || frozen != o.frozen;
        }
    };

    void setFrame (std::shared_ptr<const ScopeFrame> f) { frame = std::move (f); repaint(); }
    std::shared_ptr<const ScopeFrame> getFrame() const { return frame; }
    void setSettings (const Settings& s) { if (s != settings) { settings = s; repaint(); } }

    void paint (juce::Graphics& g) override;

private:
    std::shared_ptr<const ScopeFrame> frame;
    Settings settings;
};
