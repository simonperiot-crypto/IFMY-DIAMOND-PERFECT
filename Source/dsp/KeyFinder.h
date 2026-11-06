#pragma once
#include "FFT.h"
#include <array>
#include <algorithm>

// IFMY DIAMOND PERFECT — Key Finder
// Écoute la voix (ou l'instru en side-chain), construit un profil chromatique (12 notes)
// par FFT et le compare aux profils de tonalité majeurs / mineurs (Krumhansl-Kessler).
// Tourne dans le thread de l'interface, jamais dans le thread audio.

namespace dsp
{
    class KeyFinder
    {
    public:
        struct Result
        {
            int key = -1;          // 0 = Do
            bool minor = false;
            float confidence = 0;  // 0..1
            int secondKey = -1;
            bool secondMinor = false;
        };

        void prepare (double sampleRate)
        {
            rate = sampleRate;
            fft.prepare (kOrder);
            frame.assign ((size_t) kSize, 0.0f);
            windowed.assign ((size_t) kSize, 0.0f);
            mag.assign ((size_t) kSize / 2, 0.0f);
            window.resize ((size_t) kSize);
            for (int i = 0; i < kSize; ++i)
                window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * i / (kSize - 1));

            // Classe de hauteur de chaque bin (80 Hz .. 2,5 kHz), -1 = ignoré
            binPc.assign ((size_t) kSize / 2, -1);
            for (int b = 1; b < kSize / 2; ++b)
            {
                const double f = b * rate / kSize;
                if (f < 80.0 || f > 2500.0) continue;
                const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0));
                binPc[(size_t) b] = ((midi % 12) + 12) % 12;
            }
            reset();
        }

        void reset()
        {
            chroma.fill (0.0);
            std::fill (frame.begin(), frame.end(), 0.0f);
            fill = 0;
            frames = 0;
            energy = 0;
        }

        // Ajoute des échantillons mono (au taux donné à prepare)
        void push (const float* x, int num)
        {
            for (int i = 0; i < num; ++i)
            {
                frame[(size_t) fill++] = x[i];
                if (fill == kSize)
                {
                    analyse();
                    std::copy (frame.begin() + kHop, frame.end(), frame.begin());
                    fill = kSize - kHop;
                }
            }
        }

        int analysedFrames() const { return frames; }
        const std::array<double, 12>& profile() const { return chroma; }

        Result result() const
        {
            static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
            static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

            Result r;
            double total = 0;
            for (double c : chroma) total += c;
            if (total <= 0 || frames < 4) return r;

            struct Cand { double score; int key; bool minor; };
            std::array<Cand, 24> c {};
            for (int k = 0; k < 12; ++k)
                for (int m = 0; m < 2; ++m)
                {
                    const double* prof = m ? mnr : maj;
                    double mx = 0, my = 0;
                    for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) i]; my += prof[(i - k + 12) % 12]; }
                    mx /= 12; my /= 12;
                    double sxy = 0, sxx = 0, syy = 0;
                    for (int i = 0; i < 12; ++i)
                    {
                        const double dx = chroma[(size_t) i] - mx, dy = prof[(i - k + 12) % 12] - my;
                        sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
                    }
                    c[(size_t) (k * 2 + m)] = { (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0, k, m == 1 };
                }

            std::sort (c.begin(), c.end(), [] (const Cand& a, const Cand& b) { return a.score > b.score; });
            r.key = c[0].key; r.minor = c[0].minor;
            r.secondKey = c[1].key; r.secondMinor = c[1].minor;
            // Confiance : niveau de corrélation + écart avec la 2e tonalité + quantité d'écoute
            const double margin = std::clamp ((c[0].score - c[1].score) * 6.0, 0.0, 1.0);
            const double amount = std::clamp (frames / 40.0, 0.0, 1.0);
            r.confidence = (float) std::clamp (std::max (0.0, c[0].score) * (0.55 + 0.45 * margin) * (0.4 + 0.6 * amount), 0.0, 1.0);
            return r;
        }

        static constexpr int kOrder = 12, kSize = 1 << kOrder, kHop = kSize / 2;

    private:
        void analyse()
        {
            double e = 0;
            for (int i = 0; i < kSize; ++i) { windowed[(size_t) i] = frame[(size_t) i] * window[(size_t) i]; e += (double) frame[(size_t) i] * frame[(size_t) i]; }
            if (e / kSize < 1.0e-7) return; // silence ignoré
            fft.magnitude (windowed.data(), mag.data());

            // Garde les pics locaux (harmoniques), compression douce pour équilibrer graves / aigus
            std::array<double, 12> local {};
            for (int b = 2; b < kSize / 2 - 1; ++b)
            {
                const int pc = binPc[(size_t) b];
                if (pc < 0) continue;
                const float m = mag[(size_t) b];
                if (m > mag[(size_t) b - 1] && m >= mag[(size_t) b + 1])
                    local[(size_t) pc] += std::sqrt ((double) m);
            }
            double sum = 0;
            for (double v : local) sum += v;
            if (sum <= 0) return;
            for (int i = 0; i < 12; ++i) chroma[(size_t) i] += local[(size_t) i] / sum; // chaque trame pèse pareil
            ++frames;
        }

        FFT fft;
        double rate = 12000;
        std::vector<float> frame, windowed, mag, window;
        std::vector<int> binPc;
        std::array<double, 12> chroma {};
        int fill = 0, frames = 0;
        double energy = 0;
    };
}
