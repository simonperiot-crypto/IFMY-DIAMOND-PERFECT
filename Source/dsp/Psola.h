#pragma once
#include "DSP.h"

// IFMY DIAMOND PERFECT — moteur STUDIO : PSOLA temps réel avec contrôle des formants.
//
// Le signal est découpé en grains de 2 périodes (fenêtre de Hann), recollés à un
// nouvel espacement (= nouvelle hauteur) SANS rééchantillonnage : le timbre (formants)
// de la voix est conservé. Les grains peuvent aussi être rééchantillonnés d'un facteur
// « formant » indépendant (gorge plus longue / plus courte, voix chipmunk / grave...).
//
// Latence fixe D = 2 x demi-grain maximum (dépend de la note la plus grave de la plage).
// Aucune allocation dans process().

namespace dsp
{
    class PsolaShifter
    {
    public:
        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = std::clamp (numChannels, 1, 2);
            int sz = 16;
            while (sz < (int) (sr * 0.25)) sz <<= 1; // > 2 x latence max (50 Hz -> 40 ms)
            mask = sz - 1;
            for (int c = 0; c < 2; ++c) { in[c].assign ((size_t) sz, 0.0f); acc[c].assign ((size_t) sz, 0.0f); }
            wsum.assign ((size_t) sz, 0.0f);
            setLowestFrequency (lowestHz);
            reset();
        }

        // La note la plus grave attendue fixe la taille max des grains et donc la latence
        void setLowestFrequency (float hz)
        {
            lowestHz = std::clamp (hz, 45.0f, 400.0f);
            hcap = sr / lowestHz;
            D = (int) std::ceil (2.0 * hcap) + 8;
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c) { std::fill (in[c].begin(), in[c].end(), 0.0f); std::fill (acc[c].begin(), acc[c].end(), 0.0f); }
            std::fill (wsum.begin(), wsum.end(), 0.0f);
            n = 0;
            ts = hcap;
            taGrid = ts;
            wasVoiced = false;
        }

        int getLatency() const { return D; }

        void setPeriod (float periodSamples) { period = periodSamples > 1.0f ? periodSamples : 0.0f; }
        void setRatio (float r)             { ratio = std::clamp (r, 0.25f, 4.0f); }
        void setFormant (float f)           { formant = std::clamp (f, 0.4f, 2.5f); }

        // Traite une trame (1 échantillon par canal), en place
        inline void process (float* frame, int numCh)
        {
            numCh = std::min (numCh, channels);
            const int64_t w = n & mask;
            for (int c = 0; c < numCh; ++c) in[c][(size_t) w] = frame[c];
            ++n;

            const bool voiced = period > 0.0f;
            const double P = voiced ? (double) period : sr * 0.007;
            const double step = voiced ? P / ratio : P;

            // Génère les grains dont toute l'entrée nécessaire est disponible
            int guard = 0;
            while (ts + hcap + 4.0 <= (double) n && guard++ < 64)
            {
                makeGrain (voiced, P, numCh);
                ts += std::max (2.0, step);
            }

            // Sortie retardée de D échantillons
            const int64_t tout = n - 1 - D;
            if (tout < 0) { for (int c = 0; c < numCh; ++c) frame[c] = 0.0f; return; }
            const size_t r = (size_t) (tout & mask);
            const float norm = 1.0f / std::max (wsum[r], 0.4f);
            for (int c = 0; c < numCh; ++c) { frame[c] = acc[c][r] * norm; acc[c][r] = 0.0f; }
            wsum[r] = 0.0f;
        }

    private:
        inline float readIn (int c, double x) const
        {
            const int64_t i = (int64_t) std::floor (x);
            const float f = (float) (x - (double) i);
            const auto& b = in[c];
            const float xm1 = b[(size_t) ((i - 1) & mask)], x0 = b[(size_t) (i & mask)],
                        x1  = b[(size_t) ((i + 1) & mask)], x2 = b[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        void makeGrain (bool voiced, double P, int numCh)
        {
            // Marque d'analyse : grille espacée d'une période, toujours <= marque de synthèse
            if (voiced)
            {
                if (! wasVoiced || ts - taGrid > 4.0 * P || taGrid > ts) taGrid = ts;
                while (taGrid + P <= ts) taGrid += P;
            }
            else
            {
                taGrid = ts;
            }
            wasVoiced = voiced;

            const double Pa = std::min (P, hcap);
            const double phi = (double) formant;
            const double Hs = std::min (Pa / phi, Pa);  // demi-largeur du grain en sortie
            const int64_t j0 = (int64_t) std::ceil (ts - Hs), j1 = (int64_t) std::floor (ts + Hs);
            const double invHs = 1.0 / Hs;

            for (int64_t j = j0; j <= j1; ++j)
            {
                const double u = (double) j - ts;
                const float win = (float) (0.5 + 0.5 * std::cos (kPiD * u * invHs));
                const double x = taGrid + u * phi;
                const size_t o = (size_t) (j & mask);
                for (int c = 0; c < numCh; ++c) acc[c][o] += win * readIn (c, x);
                wsum[o] += win;
            }
        }

        std::vector<float> in[2], acc[2], wsum;
        double sr = 48000, hcap = 565, ts = 565, taGrid = 565;
        float lowestHz = 85.0f, period = 0, ratio = 1, formant = 1;
        int64_t n = 0, mask = 0;
        int channels = 2, D = 1138;
        bool wasVoiced = false;
    };
}
