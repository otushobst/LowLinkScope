#include "ScopeView.h"

#include <cmath>

namespace
{
    // Draws one signal. Band-limited data (or zoomed-in views) is drawn as one
    // continuous anti-aliased curve sampled every half pixel with linear
    // interpolation; dense full-band data is drawn as a filled min/max envelope.
    template <typename SampleFn>
    void drawSignal (juce::Graphics& g, SampleFn sample, int start, int len, int total,
                     juce::Rectangle<float> r, float amp, bool smooth, juce::Colour colour)
    {
        if (len < 2 || total < 2)
            return;

        const float w = r.getWidth();
        const float cy = r.getCentreY();
        const float half = r.getHeight() * 0.5f;
        auto toY = [&] (float v)
        {
            return juce::jlimit (r.getY(), r.getBottom(), cy - v * amp * half);
        };
        const float samplesPerPixel = float (len) / juce::jmax (1.0f, w);

        if (smooth || samplesPerPixel <= 2.0f)
        {
            const int steps = juce::jmax (2, static_cast<int> (w * 2.0f));
            juce::Path p;
            p.preallocateSpace (steps * 3 + 8);
            for (int j = 0; j <= steps; ++j)
            {
                const float t = float (j) / float (steps);
                const float fpos = float (start) + t * float (len - 1);
                const int i0 = juce::jlimit (0, total - 1, static_cast<int> (fpos));
                const int i1 = juce::jmin (i0 + 1, total - 1);
                const float frac = fpos - float (i0);
                const float v = sample (i0) + (sample (i1) - sample (i0)) * frac;
                const float x = r.getX() + t * w;
                if (j == 0)
                    p.startNewSubPath (x, toY (v));
                else
                    p.lineTo (x, toY (v));
            }
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        // Dense full-band data: min/max envelope as one filled shape.
        const int cols = juce::jmax (1, static_cast<int> (w));
        std::vector<float> hi (static_cast<size_t> (cols)), lo (static_cast<size_t> (cols));
        for (int x = 0; x < cols; ++x)
        {
            const int s0 = start + static_cast<int> ((int64_t (x) * len) / cols);
            const int s1 = juce::jmin (total, juce::jmax (s0 + 1, start + static_cast<int> ((int64_t (x + 1) * len) / cols)));
            float mn = 1.0e9f, mx = -1.0e9f;
            for (int s = juce::jmin (s0, total - 1); s < s1; ++s)
            {
                const float v = sample (s);
                mn = juce::jmin (mn, v);
                mx = juce::jmax (mx, v);
            }
            hi[size_t (x)] = toY (mx);
            lo[size_t (x)] = toY (mn);
        }
        juce::Path env;
        env.preallocateSpace (cols * 6 + 8);
        env.startNewSubPath (r.getX(), hi[0]);
        for (int x = 1; x < cols; ++x)
            env.lineTo (r.getX() + float (x) + 0.5f, hi[size_t (x)]);
        for (int x = cols - 1; x >= 0; --x)
            env.lineTo (r.getX() + float (x) + 0.5f, lo[size_t (x)] + 0.6f);
        env.closeSubPath();
        g.setColour (colour.withMultipliedAlpha (0.55f));
        g.fillPath (env);
        g.setColour (colour);
        g.strokePath (env, juce::PathStrokeType (0.8f, juce::PathStrokeType::curved));
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
    auto sampleToX = [&] (double s) { return r.getX() + float ((s - start) / double (len)) * r.getWidth(); };

    // Beat grid
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
    const bool sameLen = b.size() == a.size();
    const bool showA = frame->hasA && settings.channel != 2;
    const bool showB = frame->hasB && settings.channel != 0 && sameLen;

    // Conflict overlay: both waves meaningful and in opposite polarity, smoothed
    // across neighbouring columns so it reads as soft bands instead of stripes.
    if (frame->hasA && frame->hasB && sameLen && settings.channel == 1)
    {
        const float thrA = 0.08f * peakOf (a);
        const float thrB = 0.08f * peakOf (b);
        const int cols = juce::jmax (1, static_cast<int> (r.getWidth()));
        std::vector<float> amount (static_cast<size_t> (cols), 0.0f);
        for (int x = 0; x < cols; ++x)
        {
            const int s0 = start + static_cast<int> ((int64_t (x) * len) / cols);
            const int s1 = juce::jmin (total, juce::jmax (s0 + 1, start + static_cast<int> ((int64_t (x + 1) * len) / cols)));
            int bad = 0, count = 0;
            for (int s = juce::jmin (s0, total - 1); s < s1; ++s)
            {
                ++count;
                const float va = a[size_t (s)], vb = b[size_t (s)];
                if (std::abs (va) > thrA && std::abs (vb) > thrB && va * vb < 0.0f)
                    ++bad;
            }
            amount[size_t (x)] = count > 0 ? float (bad) / float (count) : 0.0f;
        }
        const int radius = 8;
        for (int x = 0; x < cols; ++x)
        {
            float acc = 0.0f;
            int n = 0;
            for (int k = juce::jmax (0, x - radius); k <= juce::jmin (cols - 1, x + radius); ++k)
            {
                acc += amount[size_t (k)];
                ++n;
            }
            const float v = acc / float (n);
            if (v > 0.01f)
            {
                g.setColour (Colours_::conflict.withAlpha (0.28f * juce::jmin (1.0f, v * 1.4f)));
                g.fillRect (r.getX() + float (x), r.getY(), 1.0f, r.getHeight());
            }
        }
    }

    const float amp = settings.amp;
    const bool smooth = frame->lowpassed;
    if (showB && settings.linkLevel > 0.0f)
        drawSignal (g, [&] (int i) { return b[size_t (i)]; }, start, len, total, r, amp, smooth,
                    Colours_::link.withAlpha (0.92f * settings.linkLevel));
    if (showA && settings.selfLevel > 0.0f)
        drawSignal (g, [&] (int i) { return a[size_t (i)]; }, start, len, total, r, amp, smooth,
                    Colours_::self.withAlpha (0.9f * settings.selfLevel));
    if (settings.mix && frame->hasA)
    {
        const bool withB = frame->hasB && sameLen;
        drawSignal (g, [&] (int i) { return a[size_t (i)] + (withB ? b[size_t (i)] : 0.0f); },
                    start, len, total, r, amp, smooth, Colours_::sum.withAlpha (0.8f));
    }

    // Live sweep head: dim the previous pass after the head, draw the head line.
    if (frame->headFrac >= 0.0f && ! settings.frozen)
    {
        const float hx = sampleToX (double (frame->headFrac) * total);
        if (hx < r.getRight())
        {
            const float from = juce::jmax (r.getX(), hx);
            g.setColour (Colours_::bg.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (from, r.getY(), r.getRight(), r.getBottom()));
        }
        if (hx >= r.getX() && hx <= r.getRight())
        {
            g.setColour (Colours_::text.withAlpha (0.35f));
            g.fillRect (hx - 0.5f, r.getY(), 1.0f, r.getHeight());
        }
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
