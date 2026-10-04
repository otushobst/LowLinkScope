// Standalone tests for the analysis + link layer (no JUCE).
#define _USE_MATH_DEFINES
#include "Analysis.h"
#include "LinkBus.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

static std::vector<float> kick (int n, double sr, double delayMs = 0.0, float gain = 1.0f)
{
    // Pitch-dropping sine with exponential decay, like a synthetic kick / 808.
    std::vector<float> x (size_t (n), 0.0f);
    const int d = int (std::lround (delayMs * 0.001 * sr));
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const int t = i - d;
        if (t < 0)
            continue;
        const double ts = t / sr;
        const double f = 48.0 + 110.0 * std::exp (-ts * 30.0);
        phase += 2.0 * M_PI * f / sr;
        x[size_t (i)] = gain * float (std::sin (phase) * std::exp (-ts * 4.0));
    }
    return x;
}

static std::vector<float> sine (int n, double sr, double hz, double phaseRad, float gain)
{
    std::vector<float> x (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        x[size_t (i)] = gain * float (std::sin (2.0 * M_PI * hz * i / sr + phaseRad));
    return x;
}

int main()
{
    const double sr = 48000.0;
    const int n = 24000; // 1 beat at 120 BPM

    {
        auto a = kick (n, sr);
        auto b = a;
        lowlink::zeroPhaseLowpass (a.data(), n, sr, 120.0);
        lowlink::zeroPhaseLowpass (b.data(), n, sr, 120.0);
        auto r = lowlink::analyse (a.data(), b.data(), n, sr, 120.0);
        CHECK (r.valid, "identical: valid");
        CHECK (std::abs (r.lossDb - 3.01) < 0.1, "identical: sum = +3 dB (got %.2f)", r.lossDb);
        CHECK (r.correlation > 0.99, "identical: correlation 1 (got %.3f)", r.correlation);
        CHECK (! r.hasSuggestion, "identical: no suggestion");
    }
    {
        auto a = sine (n, sr, 50.0, 0.0, 0.5f);
        auto b = sine (n, sr, 50.0, M_PI, 0.5f);
        auto r = lowlink::analyse (a.data(), b.data(), n, sr, 120.0);
        CHECK (r.lossDb < -40.0, "opposite polarity: heavy cancellation (got %.1f dB)", r.lossDb);
        CHECK (r.correlation < -0.99, "opposite polarity: correlation -1 (got %.3f)", r.correlation);
        CHECK (r.hasSuggestion && r.flip, "opposite polarity: suggests flip");
        CHECK (std::abs (r.lagMs) < 0.3, "opposite polarity: no shift needed (got %.2f ms)", r.lagMs);
    }
    {
        auto a = kick (n, sr, 0.0);
        auto b = kick (n, sr, 3.0, 0.8f); // linked track 3 ms late
        lowlink::zeroPhaseLowpass (a.data(), n, sr, 120.0);
        lowlink::zeroPhaseLowpass (b.data(), n, sr, 120.0);
        auto r = lowlink::analyse (a.data(), b.data(), n, sr, 120.0);
        CHECK (r.hasSuggestion, "3 ms late: has suggestion");
        CHECK (std::abs (r.lagMs + 3.0) < 0.25, "3 ms late: advance by 3 ms (got %.2f ms)", r.lagMs);
        CHECK (! r.flip, "3 ms late: no flip");
        CHECK (r.gainDb > 0.5, "3 ms late: gain %.2f dB", r.gainDb);
    }
    {
        // Zero-phase filter must not shift the waveform.
        auto x = sine (n, sr, 60.0, 0.0, 1.0f);
        auto y = x;
        lowlink::zeroPhaseLowpass (y.data(), n, sr, 120.0);
        auto r = lowlink::analyse (x.data(), y.data(), n, sr, 120.0);
        CHECK (std::abs (r.lagMs) < 0.1, "zero-phase filter: no time shift (got %.3f ms)", r.lagMs);
    }
    {
        auto a = sine (n, sr, 50.0, 0.0, 0.5f);
        std::vector<float> b (size_t (n), 0.0f);
        auto r = lowlink::analyse (a.data(), b.data(), n, sr, 120.0);
        CHECK (! r.valid, "silent link track: not valid");
    }

    // Link bus: two independent mappings of the same shared memory
    {
        lowlink::LinkBus w, rd;
        CHECK (w.isOpen() && rd.isOpen(), "link bus opens (%s)", w.getError().c_str());
        const int sa = w.claimSlot (111);
        const int sb = rd.claimSlot (222);
        CHECK (sa >= 0 && sb >= 0 && sa != sb, "two distinct slots (%d, %d)", sa, sb);
        w.setName (sa, "Kick");

        std::vector<float> block (512);
        const int64_t base = 1000000; // timeline position
        for (int k = 0; k < 100; ++k)
        {
            for (int i = 0; i < 512; ++i)
                block[size_t (i)] = float (k * 512 + i);
            w.write (sa, block.data(), 512, base + k * 512);
            w.publishMeta (sa, sr, 120.0, 0.0, base + (k + 1) * 512, lowlink::timeline, true);
        }

        bool found = false;
        for (auto& info : rd.listAlive())
            if (info.index == sa && info.name == "Kick" && info.instanceId == 111)
                found = true;
        CHECK (found, "reader sees 'Kick' slot");

        std::vector<float> out (1000);
        const int got = rd.readRange (sa, base + 20000, 1000, out.data());
        bool exact = got == 1000;
        for (int i = 0; exact && i < 1000; ++i)
            exact = out[size_t (i)] == float (20000 + i);
        CHECK (exact, "readRange returns exact samples at timeline position");

        const int partial = rd.readRange (sa, base + 51000, 1000, out.data());
        CHECK (partial == 200 && out[999] == 0.0f, "unwritten tail reads as silence (%d valid)", partial);

        w.releaseSlot (sa, 111);
        rd.releaseSlot (sb, 222);
        lowlink::SlotInfo tmp;
        CHECK (! rd.getInfo (sa, tmp), "released slot disappears");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
