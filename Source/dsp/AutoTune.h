#pragma once
#include "DSP.h"
#include "Psola.h"
#include "Scales.h"

// IFMY DIAMOND PERFECT — moteur d'autotune temps réel (module TUNE), Phase 2
//
//  Deux moteurs, toujours alimentés (passage de l'un à l'autre en fondu, sans clic) :
//   - STUDIO : PSOLA, formants conservés, gorge réglable, latence ~ 2 périodes de la note la plus grave
//   - LIVE   : double tête de lecture courte, latence ~3 à 6 ms pour chanter en direct
//
//  Détection : YIN + plage vocale + anti-saut d'octave + médiane sur 3 mesures.
//  Correction : modes Auto / Classic, vitesse, intensité, humanize, Flex-Tune, transpose,
//  vibrato ajouté, référence 430-450 Hz, notes forcées au clavier MIDI.
//  Aucune allocation dans process().

namespace dsp
{
    struct VoiceRange { const char* name; float minHz, maxHz; };

    inline const VoiceRange kRanges[] =
    {
        { "Soprano",      160.0f, 1400.0f },
        { "Alto / Tenor",  85.0f, 1000.0f },
        { "Baryton",       65.0f,  700.0f },
        { "Basse",         50.0f,  500.0f },
        { "Instrument",    50.0f, 1400.0f },
    };
    constexpr int kNumRanges = 5;
    constexpr int kDefaultRange = 1;

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
            bool     classic   = false;   // mode Classic (correction « vintage »)
            bool     live      = false;   // moteur LIVE (faible latence) au lieu de STUDIO
            float    flex      = 0.0f;    // Flex-Tune 0..1
            bool     formant   = true;    // conserver les formants (STUDIO)
            float    throat    = 1.0f;    // longueur de gorge 0.6..1.4
            float    vibDepth  = 0.0f;    // vibrato ajouté, demi-tons crête
            float    vibRate   = 5.5f;    // Hz
            float    vibDelay  = 250.0f;  // ms après le début de la note
            int      range     = kDefaultRange;
        };

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate;
            channels = std::clamp (numChannels, 1, 2);
            range = pendingRange = std::clamp (prm.range, 0, kNumRanges - 1);

            det.prepare (sr);
            shifter.prepare (sr, channels);
            shifter.setLive (true);
            psola.prepare (sr, channels);
            psola.setLowestFrequency (kRanges[range].minHz);
            psola.reset();
            configureDetector();

            latLive = (int) std::lround (sr * 0.003) + 2;
            fixed.resize ((size_t) channels);
            for (auto& f : fixed) f.prepare ((int) (sr * 0.06) + 64);

            corr = 0; detected = 0; target = -1; voiced = false;
            unvoicedSamples = holdSamples = noteSamples = 0;
            octaveStreak = 0; lastStable = -1; histCount = 0;
            vibPhase = 0;
            mix = isActive() ? 1.0f : 0.0f;
            mixStep = 1.0f / (float) (sr * 0.02);
            engineMix = prm.live ? 0.0f : 1.0f;
            engineStep = 1.0f / (float) (sr * 0.025);
            mute = 1.0f;
            muteStep = 1.0f / (float) (sr * 0.008);
            setParams (prm);
        }

        // Latence que l'hôte doit compenser (selon le moteur et la plage choisis)
        int getLatency() const { return prm.live ? latLive : studioLatency (pendingRange); }

        int studioLatency (int r) const { return (int) std::ceil (2.0 * sr / kRanges[std::clamp (r, 0, kNumRanges - 1)].minHz) + 8; }

        void setParams (const Params& p)
        {
            const bool liveChanged = p.live != prm.live;
            prm = p;
            prm.refHz  = std::clamp (prm.refHz, 400.0f, 480.0f);
            prm.throat = std::clamp (prm.throat, 0.5f, 1.6f);
            prm.flex   = std::clamp (prm.flex, 0.0f, 1.0f);
            pendingRange = std::clamp (p.range, 0, kNumRanges - 1);
            absMask = p.midiMask != 0 ? p.midiMask : absoluteMask (p.key, p.scale);
            coefFast = coefFromMs (p.retuneMs, sr);
            coefHeld = coefFromMs (p.retuneMs + p.humanize * 300.0f, sr);
            classicStep = (float) (1000.0 / (sr * std::max (0.5, (double) p.retuneMs))); // demi-tons / échantillon
            if (liveChanged) configureDetector();
        }

        inline void process (float* frame, int numCh)
        {
            numCh = std::min (numCh, channels);

            // --- Changement de plage vocale : fondu au silence, reconfiguration, retour
            if (pendingRange != range)
            {
                mute = std::max (0.0f, mute - muteStep);
                if (mute <= 0.0f)
                {
                    range = pendingRange;
                    psola.setLowestFrequency (kRanges[range].minHz);
                    psola.reset();
                    configureDetector();
                }
            }
            else if (mute < 1.0f) mute = std::min (1.0f, mute + muteStep);

            // --- Détection
            const float mono = numCh > 1 ? 0.5f * (frame[0] + frame[1]) : frame[0];
            if (det.push (mono)) onDetection();

            // --- Correction
            float desired = 0.0f;
            if (voiced && prm.on)
            {
                const float e = (float) target - detected;
                float g = 1.0f;
                if (prm.flex > 0.0f)
                {
                    const float t = std::clamp (std::abs (e) / 0.5f, 0.0f, 1.0f);
                    g = 1.0f - prm.flex * t * t * (3.0f - 2.0f * t);
                }
                desired = e * prm.amount * g;
            }

            ++holdSamples;
            ++noteSamples;
            if (prm.classic)
                corr += std::clamp (desired - corr, -classicStep, classicStep);
            else
            {
                const float c = holdSamples > (int) (sr * 0.15) ? coefHeld : coefFast;
                corr += (desired - corr) * c;
            }

            // --- Vibrato ajouté (démarre après le délai, montée en 300 ms)
            float vib = 0.0f;
            if (prm.on && prm.vibDepth > 0.0f && voiced)
            {
                vibPhase += prm.vibRate / (float) sr;
                if (vibPhase >= 1.0f) vibPhase -= 1.0f;
                const float t = ((float) noteSamples - prm.vibDelay * 0.001f * (float) sr) / (float) (sr * 0.3);
                vib = prm.vibDepth * std::clamp (t, 0.0f, 1.0f) * std::sin (2.0f * kPi * vibPhase);
            }

            const float shift = std::clamp (corr + prm.transpose + vib, -36.0f, 36.0f);
            mix = isActive() ? std::min (1.0f, mix + mixStep) : std::max (0.0f, mix - mixStep);
            const float ratio = std::exp2 (shift / 12.0f);

            // --- Moteur LIVE
            shifter.setRatio (ratio);
            shifter.setDrift (! voiced);
            shifter.advance();

            // --- Moteur STUDIO
            psola.setRatio (ratio);
            psola.setFormant ((prm.formant ? 1.0f : ratio) / prm.throat);
            psola.setPeriod (voiced ? (float) (sr / detectedHz) : 0.0f);
            float studio[2] = { frame[0], numCh > 1 ? frame[1] : 0.0f };
            psola.process (studio, numCh);

            const float engTarget = prm.live ? 0.0f : 1.0f;
            if (engineMix < engTarget) engineMix = std::min (engTarget, engineMix + engineStep);
            else if (engineMix > engTarget) engineMix = std::max (engTarget, engineMix - engineStep);

            const int latS = psola.getLatency();
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = frame[ch];
                fixed[(size_t) ch].push (x);
                const float dry = fixed[(size_t) ch].readInt (latLive) * (1.0f - engineMix)
                                + fixed[(size_t) ch].readInt (latS) * engineMix;
                const float wetLive = shifter.process (ch, x);
                const float wet = wetLive + engineMix * (studio[ch] - wetLive);
                frame[ch] = (dry + mix * (wet - dry)) * mute;
            }

            outShift = shift;
        }

        // Infos pour l'interface (lues par le thread graphique, sans verrou)
        std::atomic<float> uiDetected { -1.0f }, uiOutput { -1.0f }, uiTarget { -1.0f };

        bool inScale (int note) const   { return (absMask >> (((note % 12) + 12) % 12)) & 1u; }
        uint16_t currentMask() const    { return absMask; }
        float midiFromHz (float hz) const { return 69.0f + 12.0f * std::log2 (hz / prm.refHz); }

    private:
        bool isActive() const { return prm.on || std::abs (prm.transpose) > 0.001f || std::abs (prm.throat - 1.0f) > 0.005f; }

        void configureDetector()
        {
            const auto& r = kRanges[std::clamp (range, 0, kNumRanges - 1)];
            det.configure (r.minHz, r.maxHz, prm.live ? 12.0f : 20.0f);
        }

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
            const auto& rg = kRanges[std::clamp (range, 0, kNumRanges - 1)];
            if (det.freq >= rg.minHz * 0.95f && det.freq <= rg.maxHz * 1.05f)
            {
                float midi = midiFromHz (det.freq);

                // Anti-saut d'octave : il faut 3 mesures d'affilée pour accepter un saut d'une octave
                if (lastStable > 0.0f)
                {
                    const float d = midi - lastStable;
                    if (std::abs (std::abs (d) - 12.0f) < 0.8f)
                    {
                        if (++octaveStreak < 3) midi -= 12.0f * (d > 0 ? 1.0f : -1.0f);
                        else octaveStreak = 0;
                    }
                    else octaveStreak = 0;
                }

                // Médiane sur 3 mesures (supprime les pointes isolées)
                hist[histCount % 3] = midi;
                ++histCount;
                if (histCount >= 3)
                {
                    const float a = hist[0], b = hist[1], c = hist[2];
                    midi = std::max (std::min (a, b), std::min (std::max (a, b), c));
                }
                lastStable = midi;

                const int cand = nearestNote (midi);
                const float hyst = prm.classic ? 0.0f : 0.12f;
                if (target < 0 || ! inScale (target))
                {
                    if (target != cand) noteSamples = 0;
                    target = cand;
                    holdSamples = 0;
                }
                else if (cand != target)
                {
                    const float dCand = std::abs (midi - (float) cand);
                    const float dTarg = std::abs (midi - (float) target);
                    if (dTarg - dCand > hyst) { target = cand; holdSamples = 0; noteSamples = 0; }
                }

                if (! voiced) noteSamples = 0;
                detected = midi;
                detectedHz = prm.refHz * std::exp2 ((midi - 69.0f) / 12.0f);
                voiced = true;
                unvoicedSamples = 0;
                shifter.setPeriod ((float) (sr / detectedHz));

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
                    histCount = 0;
                    shifter.setPeriod (0);
                    if (unvoicedSamples > 40) { target = -1; lastStable = -1; }
                    uiDetected.store (-1.0f, std::memory_order_relaxed);
                    uiOutput.store (-1.0f, std::memory_order_relaxed);
                }
            }
        }

        PitchDetector det;
        PitchShifter shifter;
        PsolaShifter psola;
        std::vector<DelayLine> fixed;
        Params prm;
        double sr = 48000;
        int channels = 2, range = kDefaultRange, pendingRange = kDefaultRange, latLive = 146;
        uint16_t absMask = 0x0FFF;
        float coefFast = 1, coefHeld = 1, classicStep = 1, corr = 0, detected = 0, detectedHz = 220, outShift = 0;
        float mix = 1, mixStep = 0.001f, engineMix = 1, engineStep = 0.001f, mute = 1, muteStep = 0.003f;
        float vibPhase = 0, lastStable = -1, hist[3] {};
        int target = -1, unvoicedSamples = 0, holdSamples = 0, noteSamples = 0, octaveStreak = 0, histCount = 0;
        bool voiced = false;
    };
}
