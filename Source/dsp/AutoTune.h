#pragma once
#include "DSP.h"
#include "Scales.h"

// IFMY DIAMOND PERFECT — moteur d'autotune temps réel (module TUNE)
// Détection YIN + pitch shifter à fenêtre calée sur la période.
// Aucune allocation dans process() : tout est préparé dans prepare().

namespace dsp
{
    class AutoTune
    {
    public:
        struct Params
        {
            bool     on        = true;
            int      key       = 0;       // 0 = C
            uint16_t scale     = 0x0FFF;  // masque relatif à la tonique
            float    retuneMs  = 40.0f;   // 0 = hard tune
            float    amount    = 1.0f;    // intensité 0..1
            float    humanize  = 0.0f;    // 0..1
            float    transpose = 0.0f;    // demi-tons
            float    refHz     = 440.0f;  // La de référence (430..450)
            uint16_t midiMask  = 0;       // notes tenues au clavier (classes de hauteur), 0 = inactif
        };

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = std::max (1, numChannels);
            det.prepare (sr);
            shifter.prepare (sr, channels);
            latency = (int) std::lround (sr * 0.010);
            fixed.resize ((size_t) channels);
            for (auto& f : fixed) f.prepare (latency + 64);
            corr = 0; detected = 0; target = -1; voiced = false;
            unvoicedSamples = holdSamples = 0;
            mix = prm.on ? 1.0f : 0.0f;
            mixStep = 1.0f / (float) (sr * 0.02);
            setParams (prm);
        }

        int getLatency() const { return latency; }

        void setParams (const Params& p)
        {
            prm = p;
            prm.refHz = std::clamp (prm.refHz, 400.0f, 480.0f);
            absMask = p.midiMask != 0 ? p.midiMask : absoluteMask (p.key, p.scale);
            coefFast = coefFromMs (p.retuneMs, sr);
            coefHeld = coefFromMs (p.retuneMs + p.humanize * 300.0f, sr);
        }

        inline void process (float* frame, int numCh)
        {
            numCh = std::min (numCh, channels);
            const float mono = numCh > 1 ? 0.5f * (frame[0] + frame[1]) : frame[0];
            if (det.push (mono)) onDetection();

            const float desired = (voiced && prm.on) ? ((float) target - detected) * prm.amount : 0.0f;
            ++holdSamples;
            const float c = holdSamples > (int) (sr * 0.15) ? coefHeld : coefFast;
            corr += (desired - corr) * c;

            const float shift = std::clamp (corr + prm.transpose, -36.0f, 36.0f);
            const bool active = prm.on || std::abs (prm.transpose) > 0.001f;
            mix = active ? std::min (1.0f, mix + mixStep) : std::max (0.0f, mix - mixStep);

            shifter.setRatio (std::exp2 (shift / 12.0f));
            shifter.setDrift (! voiced);
            shifter.advance();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = frame[ch];
                fixed[(size_t) ch].push (x);
                const float dry = fixed[(size_t) ch].readInt (latency);
                if (mix > 0.0f)
                {
                    const float wet = shifter.process (ch, x);
                    frame[ch] = dry + mix * (wet - dry);
                }
                else
                {
                    shifter.pushOnly (ch, x);
                    frame[ch] = dry;
                }
            }

            outShift = shift;
        }

        // Infos pour l'interface (lues par le thread graphique, sans verrou)
        std::atomic<float> uiDetected { -1.0f }, uiOutput { -1.0f }, uiTarget { -1.0f };

        bool inScale (int note) const { return (absMask >> (((note % 12) + 12) % 12)) & 1u; }
        uint16_t currentMask() const   { return absMask; }

        float midiFromHz (float hz) const { return 69.0f + 12.0f * std::log2 (hz / prm.refHz); }

    private:
        int nearestNote (float midi) const
        {
            if (absMask == 0) return (int) std::lround (midi);
            const int r = (int) std::lround (midi);
            int best = r;
            float bestDist = 1.0e9f;
            for (int n = r - 7; n <= r + 7; ++n)
            {
                if (! inScale (n)) continue;
                const float dist = std::abs (midi - (float) n);
                if (dist < bestDist) { bestDist = dist; best = n; }
            }
            return best;
        }

        void onDetection()
        {
            if (det.freq > 50.0f && det.freq < 1400.0f)
            {
                const float midi = midiFromHz (det.freq);
                const int cand = nearestNote (midi);

                if (target < 0 || ! inScale (target))
                {
                    target = cand;
                    holdSamples = 0;
                }
                else if (cand != target)
                {
                    const float dCand = std::abs (midi - (float) cand);
                    const float dTarg = std::abs (midi - (float) target);
                    if (dTarg - dCand > 0.12f) { target = cand; holdSamples = 0; }
                }

                detected = midi;
                voiced = true;
                unvoicedSamples = 0;
                shifter.setPeriod ((float) (sr / det.freq));

                uiDetected.store (midi, std::memory_order_relaxed);
                uiTarget.store ((float) target, std::memory_order_relaxed);
                uiOutput.store (midi + outShift, std::memory_order_relaxed);
            }
            else
            {
                unvoicedSamples += 1;
                if (unvoicedSamples > 6) // ~20 ms sans hauteur
                {
                    voiced = false;
                    shifter.setPeriod (0);
                    if (unvoicedSamples > 40) target = -1;
                    uiDetected.store (-1.0f, std::memory_order_relaxed);
                    uiOutput.store (-1.0f, std::memory_order_relaxed);
                }
            }
        }

        PitchDetector det;
        PitchShifter shifter;
        std::vector<DelayLine> fixed;
        Params prm;
        double sr = 48000;
        int channels = 2;
        uint16_t absMask = 0x0FFF;
        float coefFast = 1, coefHeld = 1, corr = 0, detected = 0, mix = 1, mixStep = 0.001f, outShift = 0;
        int target = -1, latency = 480, unvoicedSamples = 0, holdSamples = 0;
        bool voiced = false;
    };
}
