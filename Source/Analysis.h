#pragma once
// Pure DSP analysis helpers (no JUCE), unit-testable.

#include <vector>

namespace lowlink
{
// Zero-phase 2nd order Butterworth low-pass (forward + backward pass), in place.
// Zero phase matters: a normal filter would shift both waves and fake a phase offset.
void zeroPhaseLowpass (float* x, int n, double sampleRate, double cutoffHz);

struct Analysis
{
    bool valid = false;

    double lossDb = 0.0;      // energy of (a+b) vs energy(a)+energy(b); < 0 = cancellation
    double correlation = 0.0; // Pearson-like, -1 .. 1

    // Suggestion: shift the *linked* track (b) by lagMs (positive = later) and
    // flip its polarity if flip == true. gainDb = expected improvement of sum energy.
    double lagMs = 0.0;
    bool flip = false;
    double gainDb = 0.0;
    bool hasSuggestion = false;
};

// a = this track, b = linked track; both already band-limited to the low end.
Analysis analyse (const float* a, const float* b, int n, double sampleRate,
                  double bandHz, double maxLagMs = 10.0);
} // namespace lowlink
