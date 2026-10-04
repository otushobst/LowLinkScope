#include "ScopeView.h"

#include <cmath>

namespace
{
    template <typename SampleFn>
    void drawSignal (juce::Graphics& g, SampleFn sample, int start, int len, int total,
                     juce::Rectangle<float> r, float amp)
    {
        const int w = juce::jmax (1, static_cast<int> (r.getWidth()));
        const float cy = r.getCentreY();
        const float half = r.getHeight() * 0.5f;
        auto toY = [&] (float v)
        {
            return juce::jlimit (r.getY(), r.getBottom(), cy - v * amp * half);
        };

        if (len < w * 2)
        {
            // Zoomed in: draw a continuous line.
            juce::Path p;
            for (int i = 0; i <= len && start + i < total; ++i)
            {
                const float x = r.getX() + (float (i) / float (juce::jmax (1, len))) * r.getWidth();
                const float y = toY (sample (start + i));
                if (i == 0)
                    p.startNewSubPath (x, y);
                else
                    p.lineTo (x, y);
            }
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
            return;
        }

        // Zoomed out: min/max per pixel column.
        for (int x = 0; x < w; ++x)
        {
            const int s0 = start + static_cast<int> ((int64_t (x) * len) / w);
            int s1 = start + static_cast<int> ((int64_t (x + 1) * len) / w);
            s1 = juce::jmin (juce::jmax (s1, s0 + 1), total);
            if (s0 >= total)
                break;
            float lo = 1.0e9f, hi = -1.0e9f;
            for (int s = s0; s < s1; ++s)
            {
                const float v = sample (s);
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
            float y1 = toY (hi), y2 = toY (lo);
            if (y2 - y1 < 1.0f)
                y2 = y1 + 1.0f;
            g.drawVerticalLine (static_cast<int> (r.getX()) + x, y1, y2);
        }
    }

    float peakOf (const std::vector<float>& v)
    {
        float p = 0.0f;
        for (auto s : v)
            p = juce::jmax (p, std::abs (s));
        return p;
    }
} // namespace

void ScopeView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours_::bg);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Colours_::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    auto r = bounds.reduced (8.0f, 22.0f);

    // Centre line
    g.setColour (Colours_::grid);
    g.drawHorizontalLine (static_cast<int> (r.getCentreY()), r.getX(), r.getRight());

    if (frame == nullptr || frame->a.empty())
    {
        g.setColour (Colours_::dim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Press play in your DAW", r, juce::Justification::centred);
        return;
    }

    const int total = static_cast<int> (frame->a.size());
    const float visibleFrac = 1.0f / juce::jmax (1.0f, settings.zoom);
    const int len = juce::jmax (2, static_cast<int> (std::round (visibleFrac * total)));
    const int start = juce::jlimit (0, juce::jmax (0, total - len),
                                    static_cast<int> (std::round (settings.position * (1.0f - visibleFrac) * total)));

    // Beat grid (1/16 notes, or beats if too dense)
    {
        const double beatsTotal = frame->sizeBeats;
        const double startBeat = beatsTotal * start / total;
        const double beatsVisible = beatsTotal * len / total;
        double step = 0.25;
        while (beatsVisible / step > 48.0)
            step *= 2.0;
        while (beatsVisible / step < 4.0 && step > 1.0 / 256.0)
            step *= 0.5;
        for (double b = std::ceil (startBeat / step) * step; b <= startBeat + beatsVisible + 1e-9; b += step)
        {
            const float x = r.getX() + float ((b - startBeat) / beatsVisible) * r.getWidth();
            const bool onBeat = std::abs (b - std::round (b)) < 1e-6;
            g.setColour (onBeat ? Colours_::panelEdge : Colours_::grid);
            g.drawVerticalLine (static_cast<int> (x), r.getY(), r.getBottom());
        }
    }

    const auto& a = frame->a;
    const auto& b = frame->b;
    const bool showA = frame->hasA && settings.channel != 2;
    const bool showB = frame->hasB && settings.channel != 0 && b.size() == a.size();

    // Conflict overlay: both waves meaningful and in opposite polarity.
    if (frame->hasA && frame->hasB && b.size() == a.size() && settings.channel == 1)
    {
        const float thrA = 0.08f * peakOf (a);
        const float thrB = 0.08f * peakOf (b);
        const int w = juce::jmax (1, static_cast<int> (r.getWidth()));
        for (int x = 0; x < w; ++x)
        {
            const int s0 = start + static_cast<int> ((int64_t (x) * len) / w);
            int s1 = start + static_cast<int> ((int64_t (x + 1) * len) / w);
            s1 = juce::jmin (juce::jmax (s1, s0 + 1), total);
            int bad = 0, count = 0;
            for (int s = s0; s < s1; ++s)
            {
                ++count;
                if (std::abs (a[size_t (s)]) > thrA && std::abs (b[size_t (s)]) > thrB
                    && a[size_t (s)] * b[size_t (s)] < 0.0f)
                    ++bad;
            }
            if (bad > 0)
            {
                g.setColour (Colours_::conflict.withAlpha (0.05f + 0.25f * float (bad) / float (count)));
                g.drawVerticalLine (static_cast<int> (r.getX()) + x, r.getY(), r.getBottom());
            }
        }
    }

    const float amp = settings.amp;
    if (showB && settings.linkLevel > 0.0f)
    {
        g.setColour (Colours_::link.withAlpha (0.9f * settings.linkLevel));
        drawSignal (g, [&] (int i) { return b[size_t (i)]; }, start, len, total, r, amp);
    }
    if (showA && settings.selfLevel > 0.0f)
    {
        g.setColour (Colours_::self.withAlpha (0.85f * settings.selfLevel));
        drawSignal (g, [&] (int i) { return a[size_t (i)]; }, start, len, total, r, amp);
    }
    if (settings.mix && frame->hasA)
    {
        const bool withB = frame->hasB && b.size() == a.size();
        g.setColour (Colours_::sum.withAlpha (0.75f));
        drawSignal (g, [&] (int i) { return a[size_t (i)] + (withB ? b[size_t (i)] : 0.0f); },
                    start, len, total, r, amp);
    }

    // Labels
    g.setFont (juce::FontOptions (12.0f));
    auto top = bounds.reduced (10.0f, 4.0f).withHeight (16.0f);
    g.setColour (Colours_::self);
    g.drawText (frame->nameA, top, juce::Justification::centredLeft);
    const float nameW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), frame->nameA) + 14.0f;
    if (frame->nameB.isNotEmpty())
    {
        g.setColour (Colours_::link);
        g.drawText (frame->nameB, top.withTrimmedLeft (nameW), juce::Justification::centredLeft);
    }

    juce::String right = settings.frozen ? "FROZEN" : frame->status;
    if (right.isNotEmpty())
    {
        g.setColour (settings.frozen ? Colours_::link : Colours_::dim);
        g.drawText (right, top, juce::Justification::centredRight);
    }

    if (settings.zoom > 1.01f)
    {
        g.setColour (Colours_::dim);
        g.drawText (juce::String (settings.zoom, 1) + "x",
                    bounds.reduced (10.0f, 4.0f).removeFromBottom (16.0f),
                    juce::Justification::centredRight);
    }
}
