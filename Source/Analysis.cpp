#include "Analysis.h"

#include <algorithm>
#include <cmath>

namespace lowlink
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        void setLowpass (double sr, double fc)
        {
            fc = std::clamp (fc, 5.0, sr * 0.45);
            const double w0 = 2.0 * kPi * fc / sr;
            const double cw = std::cos (w0), sw = std::sin (w0);
            const double q = 0.70710678118654752;
            const double alpha = sw / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = (1.0 - cw) * 0.5 / a0;
            b1 = (1.0 - cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }

        // Transposed direct form II
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
} // namespace

void zeroPhaseLowpass (float* x, int n, double sampleRate, double cutoffHz)
{
    if (n <= 0 || sampleRate <= 0.0)
        return;

    Biquad f;
    f.setLowpass (sampleRate, cutoffHz);
    for (int i = 0; i < n; ++i)
        x[i] = static_cast<float> (f.process (x[i]));

    Biquad r;
    r.setLowpass (sampleRate, cutoffHz);
    for (int i = n - 1; i >= 0; --i)
        x[i] = static_cast<float> (r.process (x[i]));
}

Analysis analyse (const float* a, const float* b, int n, double sampleRate,
                  double bandHz, double maxLagMs)
{
    Analysis res;
    if (n < 16 || sampleRate <= 0.0)
        return res;

    double ea = 0, eb = 0, c0 = 0;
    for (int i = 0; i < n; ++i)
    {
        ea += double (a[i]) * a[i];
        eb += double (b[i]) * b[i];
        c0 += double (a[i]) * b[i];
    }

    // Need meaningful signal in both tracks (about -70 dBFS RMS).
    const double minEnergy = double (n) * 1.0e-7;
    if (ea < minEnergy || eb < minEnergy)
        return res;

    res.valid = true;
    const double sumE = std::max (ea + eb + 2.0 * c0, 1.0e-30);
    res.lossDb = std::max (-60.0, 10.0 * std::log10 (sumE / (ea + eb)));
    res.correlation = std::clamp (c0 / std::sqrt (ea * eb), -1.0, 1.0);

    // Decimate (signals are already low-passed) so the lag search stays cheap.
    const int dec = std::max (1, static_cast<int> (std::floor (sampleRate / (std::max (bandHz, 20.0) * 10.0))));
    const int m = n / dec;
    if (m < 8)
        return res;

    std::vector<double> da (static_cast<size_t> (m)), db (static_cast<size_t> (m));
    double ead = 0, ebd = 0;
    for (int i = 0; i < m; ++i)
    {
        da[size_t (i)] = a[i * dec];
        db[size_t (i)] = b[i * dec];
        ead += da[size_t (i)] * da[size_t (i)];
        ebd += db[size_t (i)] * db[size_t (i)];
    }

    const double decRate = sampleRate / dec;
    const int maxLag = std::max (1, static_cast<int> (std::ceil (maxLagMs * 0.001 * decRate)));
    std::vector<double> c (static_cast<size_t> (2 * maxLag + 1));

    // c(l) = sum a[i] * b[i - l]  -> b delayed by l samples
    for (int l = -maxLag; l <= maxLag; ++l)
    {
        double acc = 0;
        const int iStart = std::max (0, l);
        const int iEnd = std::min (m, m + l);
        for (int i = iStart; i < iEnd; ++i)
            acc += da[size_t (i)] * db[size_t (i - l)];
        c[size_t (l + maxLag)] = acc;
    }

    // Sum energy for shift l with polarity s: ea + eb + 2 * s * c(l). Maximise s * c(l).
    int best = maxLag;
    double bestVal = -1.0e300;
    for (int k = 0; k < int (c.size()); ++k)
    {
        const double v = std::abs (c[size_t (k)]);
        if (v > bestVal)
        {
            bestVal = v;
            best = k;
        }
    }

    // Several shifts are often almost equally good (periodic bass): prefer the
    // smallest shift whose result is within 0.15 dB of the best one.
    {
        const double bestAfter = ead + ebd + 2.0 * bestVal;
        const double tolerance = bestAfter * (1.0 - std::pow (10.0, -0.015));
        int chosen = best;
        for (int k = 0; k < int (c.size()); ++k)
        {
            const double after = ead + ebd + 2.0 * std::abs (c[size_t (k)]);
            const bool isLocalPeak = (k == 0 || std::abs (c[size_t (k)]) >= std::abs (c[size_t (k - 1)]))
                                  && (k == int (c.size()) - 1 || std::abs (c[size_t (k)]) >= std::abs (c[size_t (k + 1)]));
            if (isLocalPeak && after >= bestAfter - tolerance && std::abs (k - maxLag) < std::abs (chosen - maxLag))
                chosen = k;
        }
        best = chosen;
        bestVal = std::abs (c[size_t (best)]);
    }

    const double s = c[size_t (best)] >= 0.0 ? 1.0 : -1.0;
    double frac = 0.0;
    if (best > 0 && best < int (c.size()) - 1)
    {
        const double ym = s * c[size_t (best - 1)], y0 = s * c[size_t (best)], yp = s * c[size_t (best + 1)];
        const double denom = ym - 2.0 * y0 + yp;
        if (std::abs (denom) > 1.0e-30)
            frac = std::clamp (0.5 * (ym - yp) / denom, -0.5, 0.5);
    }

    const double c0d = c[size_t (maxLag)];
    const double before = std::max (ead + ebd + 2.0 * c0d, 1.0e-30);
    const double after = std::max (ead + ebd + 2.0 * bestVal, 1.0e-30);

    res.lagMs = (best - maxLag + frac) * 1000.0 / decRate;
    res.flip = s < 0.0;
    res.gainDb = 10.0 * std::log10 (after / before);
    res.hasSuggestion = res.gainDb > 0.5 && (res.flip || std::abs (res.lagMs) > 0.05);
    return res;
}
} // namespace lowlink
