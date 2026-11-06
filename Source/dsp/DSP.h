#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <atomic>

namespace dsp
{
    constexpr double kPiD = 3.14159265358979323846;
    constexpr float  kPi  = 3.14159265358979f;

    inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
    inline float coefFromMs (double ms, double sr) { return ms <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr))); }

    //==========================================================================
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void reset() { z1 = z2 = 0; }

        inline float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        {
            b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
            a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
        }

        void lowpass (double sr, double f, double q = 0.7071)
        {
            const double w = 2 * kPiD * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
        }

        void highpass (double sr, double f, double q = 0.7071)
        {
            const double w = 2 * kPiD * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
        }

        void peak (double sr, double f, double q, double db)
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2 * kPiD * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
            set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
        }

        void highShelf (double sr, double f, double db)
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2 * kPiD * std::min (f, sr * 0.49) / sr, c = std::cos (w);
            const double a = std::sin (w) / 2 * std::sqrt (2.0), sA = 2 * std::sqrt (A) * a;
            set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                 (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
        }
    };

    //==========================================================================
    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void prepare (int minSize)
        {
            int n = 16;
            while (n < minSize) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }

        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

        inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

        // d = retard en échantillons (0 = dernier échantillon poussé), d >= 1 conseillé
        inline float read (float d) const
        {
            const float pos = (float) (w - 1) - d;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)],
                        x1  = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        inline float readInt (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    };

    //==========================================================================
    // Détection de hauteur YIN sur signal décimé (léger en CPU, précis sur la voix)
    // Mémoire allouée une fois pour la plage la plus large (50 Hz) ; la plage vocale
    // et la fenêtre se règlent ensuite en temps réel sans aucune allocation.
    class PitchDetector
    {
    public:
        void prepare (double sr)
        {
            decim  = std::max (1, (int) std::lround (sr / 11025.0));
            dsr    = sr / decim;
            maxLagAlloc = (int) std::ceil (dsr / 45.0);
            const int wAlloc = (int) std::lround (dsr * 0.030);
            sizeAlloc = wAlloc + maxLagAlloc + 2;
            hop    = std::max (8, (int) std::lround (dsr * 0.003));

            int n = 64;
            while (n < sizeAlloc * 2) n <<= 1;
            ring.assign ((size_t) n, 0.0f);
            rmask = n - 1;
            lin.assign ((size_t) sizeAlloc, 0.0f);
            diff.assign ((size_t) maxLagAlloc + 2, 1.0f);

            aa1.lowpass (sr, dsr * 0.42, 0.5412);
            aa2.lowpass (sr, dsr * 0.42, 1.3066);
            aa1.reset(); aa2.reset();
            wp = dc = hc = filled = 0;
            freq = 0; clarity = 0;
            configure (cfgMin, cfgMax, cfgWin);
        }

        // Plage de recherche (Hz) et fenêtre d'analyse (ms). Sans allocation.
        void configure (float minHz, float maxHz, float windowMs)
        {
            cfgMin = std::clamp (minHz, 45.0f, 400.0f);
            cfgMax = std::clamp (maxHz, cfgMin * 2.0f, 2000.0f);
            cfgWin = std::clamp (windowMs, 8.0f, 30.0f);
            if (ring.empty()) return;
            minLag = std::max (2, (int) std::floor (dsr / cfgMax));
            maxLag = std::min (maxLagAlloc, (int) std::ceil (dsr / cfgMin));
            W      = std::max ((int) std::lround (dsr * cfgWin * 0.001), (int) std::lround (dsr / cfgMin)); // >= 1 période
            size   = std::min (sizeAlloc, W + maxLag + 2);
            W      = size - maxLag - 2;
        }

        // renvoie true quand une nouvelle estimation est disponible
        inline bool push (float x)
        {
            const float y = aa2.process (aa1.process (x));
            if (++dc < decim) return false;
            dc = 0;
            ring[(size_t) wp] = y;
            wp = (wp + 1) & rmask;
            if (filled < sizeAlloc) ++filled;
            if (++hc < hop) return false;
            hc = 0;
            if (filled < size) { freq = 0; return true; }
            analyse();
            return true;
        }

        float freq = 0, clarity = 0;
        float threshold = 0.18f;
        double getDecimatedRate() const { return dsr; }

    private:
        void analyse()
        {
            const int start = wp - size;
            for (int j = 0; j < size; ++j)
                lin[(size_t) j] = ring[(size_t) ((start + j) & rmask)];

            double energy = 0;
            for (int j = 0; j < size; ++j) energy += (double) lin[(size_t) j] * lin[(size_t) j];
            const float rms = (float) std::sqrt (energy / size);
            if (rms < 0.0035f) { freq = 0; clarity = 0; return; } // ~ -49 dBFS : silence / souffle

            double running = 0;
            diff[0] = 1.0f;
            for (int tau = 1; tau <= maxLag; ++tau)
            {
                float s = 0;
                const float* a = lin.data();
                const float* b = lin.data() + tau;
                for (int j = 0; j < W; ++j) { const float d = a[j] - b[j]; s += d * d; }
                running += s;
                diff[(size_t) tau] = running > 0 ? (float) (s * tau / running) : 1.0f;
            }

            // 1er creux sous le seuil ; sinon le minimum global s'il reste plausible (voix soufflée)
            int best = -1, globalBest = minLag;
            for (int tau = minLag; tau <= maxLag; ++tau)
            {
                if (diff[(size_t) tau] < diff[(size_t) globalBest]) globalBest = tau;
                if (best < 0 && diff[(size_t) tau] < threshold)
                {
                    while (tau + 1 <= maxLag && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            }
            if (best < 0 && diff[(size_t) globalBest] < 0.32f) best = globalBest;
            if (best < 0) { freq = 0; clarity = 0; return; }

            float t = (float) best;
            if (best > 1 && best < maxLag)
            {
                const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
                const float den = a - 2 * b + c;
                if (std::abs (den) > 1.0e-9f) t = (float) best + std::clamp (0.5f * (a - c) / den, -1.0f, 1.0f);
            }
            freq = (float) (dsr / t);
            clarity = 1.0f - diff[(size_t) best];
        }

        Biquad aa1, aa2;
        std::vector<float> ring, lin, diff;
        double dsr = 11025;
        float cfgMin = 85.0f, cfgMax = 1000.0f, cfgWin = 20.0f;
        int decim = 4, minLag = 9, maxLag = 170, maxLagAlloc = 245, W = 220, size = 400, sizeAlloc = 600, hop = 33;
        int rmask = 0, wp = 0, dc = 0, hc = 0, filled = 0;
    };

    //==========================================================================
    // Pitch shifter à double tête de lecture, fenêtre calée sur la période (moins d'artefacts)
    class PitchShifter
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            lines.resize ((size_t) numChannels);
            for (auto& l : lines) l.prepare ((int) (sr * 0.1) + 16);
            Lnom = (float) (sr * 0.020);
            L[0] = L[1] = Lpend = Lnom;
            q[0] = 0.5f; q[1] = 0.0f;
            ratio = 1.0f;
        }

        // Mode live : fenêtre courte (1 période) = latence ~3 à 6 ms
        void setLive (bool shouldBeLive)
        {
            live = shouldBeLive;
            Lnom = (float) (sr * (live ? 0.006 : 0.020));
        }

        void setPeriod (float periodSamples)
        {
            if (periodSamples <= 1.0f) { Lpend = Lnom; return; }
            const float unit = live ? periodSamples : 2.0f * periodSamples;
            const int k = std::max (1, (int) std::lround (Lnom / unit));
            Lpend = live ? std::clamp (k * unit, (float) (sr * 0.003), (float) (sr * 0.025))
                         : std::clamp (k * unit, (float) (sr * 0.008), (float) (sr * 0.045));
        }

        void setRatio (float r)   { ratio = r; }
        void setDrift (bool shouldDrift) { drift = shouldDrift; }

        inline void advance()
        {
            for (int t = 0; t < 2; ++t)
            {
                q[t] += (1.0f - ratio) / L[t];
                if (q[t] >= 1.0f || q[t] < 0.0f)
                {
                    L[t] = Lpend;
                    float base = q[1 - t] + 0.5f;
                    base -= std::floor (base);
                    q[t] = base;
                }
            }

            // Sans hauteur détectable : on recentre doucement une seule tête (évite l'effet de filtre en peigne)
            if (drift && std::abs (ratio - 1.0f) < 1.0e-4f)
            {
                const float step = std::clamp (0.5f - q[0], -0.4f / (float) sr, 0.4f / (float) sr);
                q[0] += step;
                float o = q[0] + 0.5f;
                q[1] = o - std::floor (o);
            }

            for (int t = 0; t < 2; ++t)
            {
                d[t] = 2.0f + q[t] * L[t];
                const float s = std::sin (kPi * q[t]);
                g[t] = s * s;
            }
            const float sum = std::max (0.5f, g[0] + g[1]);
            g[0] /= sum; g[1] /= sum;
        }

        inline float process (int ch, float x)
        {
            auto& l = lines[(size_t) ch];
            l.push (x);
            return g[0] * l.read (d[0]) + g[1] * l.read (d[1]);
        }

        inline void pushOnly (int ch, float x) { lines[(size_t) ch].push (x); }

    private:
        std::vector<DelayLine> lines;
        double sr = 48000;
        float Lnom = 960, Lpend = 960, ratio = 1;
        float L[2] { 960, 960 }, q[2] { 0.5f, 0.0f }, d[2] { 2, 2 }, g[2] { 1, 0 };
        bool drift = true, live = false;
    };
}
