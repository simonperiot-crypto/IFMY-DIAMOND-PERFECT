// IFMY DIAMOND PERFECT — tests automatiques du DSP (lancés par GitHub Actions à chaque build)
// Si un test échoue, le build échoue : aucune version cassée n'arrive dans Logic.

#include "../Source/dsp/AutoTune.h"
#include "../Source/dsp/Effects.h"
#include "../Source/dsp/KeyFinder.h"
#include <cstdio>
#include <random>
#include <string>

static int failures = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.c_str());
    if (! ok) ++failures;
}

static bool finite (float x) { return std::isfinite (x) && std::abs (x) < 8.0f; }

// Mesure la hauteur moyenne d'un signal avec le détecteur (fréquences voisées uniquement)
static float measurePitch (const std::vector<float>& sig, double sr, size_t from)
{
    dsp::PitchDetector d;
    d.prepare (sr);
    double sum = 0; int count = 0;
    for (size_t i = 0; i < sig.size(); ++i)
        if (d.push (sig[i]) && i > from && d.freq > 0) { sum += d.freq; ++count; }
    return count > 0 ? (float) (sum / count) : 0.0f;
}

static std::vector<float> sine (double sr, double hz, double seconds, float amp = 0.4f)
{
    std::vector<float> v ((size_t) (sr * seconds));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2.0 * dsp::kPiD * hz * (double) i / sr);
    return v;
}

static std::vector<float> runTuner (const std::vector<float>& in, double sr, const dsp::AutoTune::Params& p, bool& allFinite,
                                     dsp::AutoTune* external = nullptr)
{
    dsp::AutoTune local;
    dsp::AutoTune& t = external != nullptr ? *external : local;
    t.setParams (p);
    t.prepare (sr, 2);
    std::vector<float> out (in.size());
    allFinite = true;
    for (size_t i = 0; i < in.size(); ++i)
    {
        float f[2] = { in[i], in[i] };
        t.process (f, 2);
        out[i] = f[0];
        if (! finite (f[0]) || ! finite (f[1])) allFinite = false;
    }
    return out;
}

int main()
{
    std::printf ("=== IFMY DIAMOND PERFECT : tests DSP ===\n");

    // 1. Détection de hauteur à toutes les fréquences d'échantillonnage
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0 })
        for (double hz : { 110.0, 220.0, 440.0, 880.0 })
        {
            auto s = sine (sr, hz, 0.5);
            const float f = measurePitch (s, sr, (size_t) (sr * 0.1));
            check (std::abs (f - hz) / hz < 0.01, "Detection " + std::to_string ((int) hz) + " Hz @ " + std::to_string ((int) sr) + " Hz -> " + std::to_string (f));
        }

    // 2. Autotune : 227 Hz (entre La et La#) en Do majeur -> doit finir sur La3 (220 Hz)
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        dsp::AutoTune::Params p;
        p.key = 0; p.scale = dsp::kScales[0].mask; p.retuneMs = 0; p.amount = 1;
        bool ok = true;
        auto out = runTuner (sine (sr, 227.0, 1.2), sr, p, ok);
        const float f = measurePitch (out, sr, (size_t) (sr * 0.5));
        check (ok, "Autotune sans NaN @ " + std::to_string ((int) sr));
        check (std::abs (f - 220.0f) / 220.0f < 0.015f, "Autotune 227 Hz -> La3 @ " + std::to_string ((int) sr) + " : " + std::to_string (f) + " Hz");
    }

    // 3. Référence 432 Hz : La4 devient 432 Hz
    {
        dsp::AutoTune::Params p; p.scale = 0x0FFF; p.retuneMs = 0; p.refHz = 432.0f;
        bool ok = true;
        auto out = runTuner (sine (48000, 438.0, 1.2), 48000, p, ok);
        const float f = measurePitch (out, 48000, 24000);
        check (std::abs (f - 432.0f) / 432.0f < 0.015f, "Reference 432 Hz : " + std::to_string (f) + " Hz");
    }

    // 4. Notes forcées au clavier MIDI (seulement Mi) : 227 Hz -> Mi3 (164.8 Hz)
    {
        dsp::AutoTune::Params p; p.retuneMs = 0; p.midiMask = (uint16_t) (1u << 4);
        bool ok = true;
        auto out = runTuner (sine (48000, 227.0, 1.2), 48000, p, ok);
        const float f = measurePitch (out, 48000, 24000);
        check (std::abs (f - 164.81f) / 164.81f < 0.02f, "Notes MIDI forcees (Mi) : " + std::to_string (f) + " Hz");
    }

    // 5. Silence, bruit et signal extrême : jamais de NaN ni d'explosion
    {
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> torture (48000 * 2);
        for (size_t i = 0; i < torture.size(); ++i)
            torture[i] = i < 24000 ? 0.0f : (i < 60000 ? u (rng) : (i % 2 ? 1.0f : -1.0f));
        dsp::AutoTune::Params p; p.transpose = 7; p.humanize = 0.5f;
        bool ok = true;
        runTuner (torture, 48000, p, ok);
        check (ok, "Autotune : silence + bruit + signal carre plein niveau");

        dsp::Gate g; g.prepare (48000); g.setThreshold (-50);
        dsp::DeEsser de; de.prepare (48000); de.setThreshold (-30);
        dsp::Compressor c; c.prepare (48000); c.set (-30, 20, 0.3f, 20, 24);
        dsp::Saturator s; s.set (24, 1);
        dsp::Doubler db; db.prepare (48000);
        dsp::StereoDelay dl; dl.prepare (48000);
        bool fx = true;
        for (float x : torture)
        {
            float f[2] = { x, -x };
            g.process (f, 2); de.process (f, 2); c.process (f, 2); s.process (f, 2);
            db.process (f, 2, 1, 1); dl.process (f, 2, 12000, 0.9f, 0.3f, 1, true);
            if (! std::isfinite (f[0]) || ! std::isfinite (f[1])) fx = false;
        }
        check (fx, "Effets (gate, de-esser, comp, saturation, doubleur, delay) sans NaN");
    }

    // 6. Gammes
    {
        check (dsp::absoluteMask (0, dsp::kScales[0].mask) == 0x0AB5, "Do majeur = C D E F G A B");
        check (dsp::absoluteMask (9, dsp::kScales[1].mask) == 0x0AB5, "La mineur = memes notes que Do majeur");
        bool allNonZero = true;
        for (auto& s : dsp::kScales) if (s.mask == 0 || ! (s.mask & 1u)) allNonZero = false;
        check (allNonZero, "Toutes les gammes contiennent leur tonique (" + std::to_string (dsp::kNumScales) + " gammes)");
    }


    // ===================== PHASE 2 =====================

    // 7. Latences des deux moteurs
    {
        dsp::AutoTune t; dsp::AutoTune::Params p;
        p.live = false; t.setParams (p); t.prepare (48000, 2);
        const double studioMs = 1000.0 * t.getLatency() / 48000.0;
        p.live = true; t.setParams (p);
        const double liveMs = 1000.0 * t.getLatency() / 48000.0;
        check (liveMs < 5.0, "Moteur LIVE : latence " + std::to_string (liveMs) + " ms (< 5 ms)");
        check (studioMs > 10.0 && studioMs < 30.0, "Moteur STUDIO : latence " + std::to_string (studioMs) + " ms");
        p.live = false; p.range = 0; t.setParams (p);
        check (t.getLatency() < (int) (48000 * 0.016), "Plage Soprano = latence STUDIO plus courte");
    }

    // 8. Moteur LIVE et mode Classic corrigent aussi 227 Hz -> La3
    for (int mode = 0; mode < 2; ++mode)
    {
        dsp::AutoTune::Params p; p.scale = dsp::kScales[0].mask; p.retuneMs = 0;
        if (mode == 0) p.live = true; else p.classic = true;
        bool ok = true;
        auto out = runTuner (sine (48000, 227.0, 1.2), 48000, p, ok);
        const float f = measurePitch (out, 48000, 30000);
        check (ok && std::abs (f - 220.0f) / 220.0f < 0.015f, std::string (mode == 0 ? "Moteur LIVE" : "Mode Classic") + " : 227 Hz -> " + std::to_string (f) + " Hz");
    }

    // Voix synthétique : impulsions glottiques filtrées par 2 formants (comme une voyelle)
    auto vowel = [] (double sr, double f0, double seconds, double formantScale = 1.0)
    {
        std::vector<float> v ((size_t) (sr * seconds));
        dsp::Biquad f1, f2;
        f1.peak (sr, 700.0 * formantScale, 6.0, 18.0);
        f2.peak (sr, 1800.0 * formantScale, 6.0, 14.0);
        dsp::Biquad lp; lp.lowpass (sr, 4000.0);
        double ph = 0;
        for (size_t i = 0; i < v.size(); ++i)
        {
            ph += f0 / sr;
            if (ph >= 1.0) ph -= 1.0;
            const float src = (float) (1.0 - 2.0 * ph) * 0.15f;  // dent de scie = riche en harmoniques
            v[i] = lp.process (f2.process (f1.process (src)));
        }
        return v;
    };

    auto centroid = [] (const std::vector<float>& x, double sr)
    {
        dsp::FFT fft; fft.prepare (13);
        const int N = 8192;
        std::vector<float> w ((size_t) N), mag ((size_t) N / 2);
        const size_t off = x.size() - (size_t) N;
        for (int i = 0; i < N; ++i) w[(size_t) i] = x[off + (size_t) i] * (0.5f - 0.5f * std::cos (2.0f * 3.14159265f * i / (N - 1)));
        fft.magnitude (w.data(), mag.data());
        double num = 0, den = 0;
        for (int b = 1; b < N / 2; ++b)
        {
            const double f = b * sr / N;
            if (f < 300 || f > 3500) continue;
            const double e = (double) mag[(size_t) b] * mag[(size_t) b];
            num += e * f; den += e;
        }
        return den > 0 ? num / den : 0.0;
    };

    // 9. Formants : +5 demi-tons avec formants conservés / sans / gorge plus longue
    {
        const double sr = 48000;
        auto in = vowel (sr, 150.0, 1.5);
        const double c0 = centroid (in, sr);
        auto run = [&] (bool keep, float throat, float transpose)
        {
            dsp::AutoTune::Params p; p.on = false; p.transpose = transpose; p.formant = keep; p.throat = throat;
            bool ok = true;
            auto out = runTuner (in, sr, p, ok);
            return std::make_pair (centroid (out, sr), ok);
        };
        auto [cKeep, ok1] = run (true, 1.0f, 5.0f);
        auto [cMove, ok2] = run (false, 1.0f, 5.0f);
        auto [cThroat, ok3] = run (true, 1.25f, 0.0f);
        check (ok1 && ok2 && ok3, "Formants : aucun NaN");
        check (std::abs (cKeep / c0 - 1.0) < 0.12, "Formants conserves (+5 st) : timbre " + std::to_string (cKeep / c0) + "x");
        check (cMove / c0 > 1.12, "Formants non conserves (+5 st) : timbre monte " + std::to_string (cMove / c0) + "x");
        check (cThroat / c0 < 0.92, "Gorge 125 % : timbre plus grave " + std::to_string (cThroat / c0) + "x");

        // la hauteur, elle, monte bien de 5 demi-tons
        dsp::AutoTune::Params p; p.on = false; p.transpose = 5;
        bool ok = true;
        auto out = runTuner (in, sr, p, ok);
        const float f = measurePitch (out, sr, 30000);
        const float expected = 150.0f * std::pow (2.0f, 5.0f / 12.0f);
        check (std::abs (f - expected) / expected < 0.015f, "Transpose +5 (STUDIO) : " + std::to_string (f) + " Hz (attendu " + std::to_string (expected) + ")");
    }

    // 10. Niveau conservé (pas de pompage) : voix juste et bruit
    {
        auto rms = [] (const std::vector<float>& v, size_t from) { double e = 0; for (size_t i = from; i < v.size(); ++i) e += (double) v[i] * v[i]; return std::sqrt (e / (double) (v.size() - from)); };
        for (int eng = 0; eng < 2; ++eng)
        {
            dsp::AutoTune::Params p; p.scale = 0x0FFF; p.live = eng == 1; p.transpose = 2;
            bool ok = true;
            auto in = vowel (48000, 196.0, 1.2);
            auto out = runTuner (in, 48000, p, ok);
            const double db = 20 * std::log10 (rms (out, 24000) / rms (in, 24000));
            check (ok && std::abs (db) < 2.5, std::string (eng ? "LIVE" : "STUDIO") + " : niveau voix conserve (" + std::to_string (db) + " dB)");

            std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.1f);
            std::vector<float> noise (48000);
            for (auto& x : noise) x = nd (rng);
            auto outN = runTuner (noise, 48000, p, ok);
            const double dbN = 20 * std::log10 (rms (outN, 12000) / rms (noise, 12000));
            check (ok && std::abs (dbN) < 3.0, std::string (eng ? "LIVE" : "STUDIO") + " : niveau souffle/consonnes conserve (" + std::to_string (dbN) + " dB)");
        }
    }

    // 11. Détection robuste : voix avec vibrato, 2e harmonique dominante et bruit -> 0 saut d'octave
    {
        const double sr = 48000;
        std::mt19937 rng (3); std::normal_distribution<float> nd (0.0f, 0.01f);
        dsp::AutoTune t; dsp::AutoTune::Params p; p.on = true; p.scale = 0x0FFF; p.retuneMs = 400;
        t.setParams (p); t.prepare (sr, 1);
        double ph = 0; int frames = 0, bad = 0, octave = 0;
        std::vector<double> truth;
        for (int i = 0; i < (int) (sr * 3); ++i)
        {
            const double f0 = 196.0 * std::pow (2.0, (0.3 * std::sin (2 * dsp::kPiD * 5.5 * i / sr) + 2.0 * std::sin (2 * dsp::kPiD * 0.25 * i / sr)) / 12.0);
            truth.push_back (f0);
            ph += f0 / sr;
            const double a = 2 * dsp::kPiD * ph;
            float x = (float) (0.12 * std::sin (a) + 0.25 * std::sin (2 * a) + 0.15 * std::sin (3 * a) + 0.08 * std::sin (4 * a) + 0.05 * std::sin (5 * a)) + nd (rng);
            t.process (&x, 1);
            if (i > sr * 0.4 && i % 256 == 0)
            {
                const float d = t.uiDetected.load();
                if (d <= 0) { ++bad; ++frames; continue; }
                const double ref = 69.0 + 12.0 * std::log2 (truth[(size_t) (i - (int) (sr * 0.022))] / 440.0);
                const double err = std::abs (d - ref);
                ++frames;
                if (err > 0.6) ++bad;
                if (err > 6.0) ++octave;
            }
        }
        check (octave == 0, "Detection robuste : aucun saut d'octave (" + std::to_string (octave) + ")");
        check (bad < frames * 3 / 100, "Detection robuste : " + std::to_string (frames - bad) + "/" + std::to_string (frames) + " mesures justes a 0.6 st");
    }

    // 12. Flex-Tune : une note chantée 0.35 st trop haut reste expressive avec Flex 100 %
    {
        const float in = 220.0f * std::pow (2.0f, 0.35f / 12.0f);
        auto errSt = [&] (float flex)
        {
            dsp::AutoTune::Params p; p.scale = 0x0FFF; p.retuneMs = 0; p.flex = flex;
            bool ok = true;
            auto out = runTuner (sine (48000, in, 1.2), 48000, p, ok);
            return 12.0f * std::log2 (measurePitch (out, 48000, 30000) / 220.0f);
        };
        const float e0 = errSt (0.0f), e1 = errSt (1.0f);
        check (std::abs (e0) < 0.06f, "Flex 0 % : corrige a fond (ecart " + std::to_string (e0) + " st)");
        check (e1 > 0.12f, "Flex 100 % : laisse l'expression (ecart " + std::to_string (e1) + " st)");
    }

    // 13. Vibrato ajouté : profondeur mesurée
    {
        dsp::AutoTune::Params p; p.scale = 0x0FFF; p.retuneMs = 0; p.vibDepth = 0.5f; p.vibRate = 5.0f; p.vibDelay = 0;
        bool ok = true;
        auto out = runTuner (sine (48000, 220.0, 2.0), 48000, p, ok);
        dsp::PitchDetector d; d.prepare (48000);
        double s = 0, s2 = 0; int n = 0;
        for (size_t i = 0; i < out.size(); ++i)
            if (d.push (out[i]) && i > 48000 && d.freq > 0)
            {
                const double st = 12.0 * std::log2 (d.freq / 220.0);
                s += st; s2 += st * st; ++n;
            }
        const double sd = n > 0 ? std::sqrt (std::max (0.0, s2 / n - (s / n) * (s / n))) : 0.0;
        check (ok && sd > 0.2 && sd < 0.5, "Vibrato 0.5 st : ecart-type mesure " + std::to_string (sd) + " st");
    }

    // 14. Changement de plage vocale en cours de lecture : pas de NaN, pas d'explosion
    {
        dsp::AutoTune t; dsp::AutoTune::Params p; t.setParams (p); t.prepare (48000, 2);
        auto s = sine (48000, 330.0, 1.0);
        bool ok = true; float peak = 0;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (i == 20000) { p.range = 0; t.setParams (p); }
            if (i == 30000) { p.live = true; t.setParams (p); }
            if (i == 40000) { p.range = 3; p.live = false; t.setParams (p); }
            float f[2] = { s[i], s[i] };
            t.process (f, 2);
            if (! finite (f[0])) ok = false;
            peak = std::max (peak, std::abs (f[0]));
        }
        check (ok && peak < 1.0f, "Changements plage/moteur en direct : stable (crete " + std::to_string (peak) + ")");
    }

    // 15. Key Finder sur des suites d'accords
    {
        const double kr = 12000;
        auto chordProg = [&] (std::vector<std::vector<int>> chords)
        {
            std::vector<float> v;
            for (int rep = 0; rep < 3; ++rep)
                for (auto& ch : chords)
                    for (int i = 0; i < (int) kr; ++i)
                    {
                        double s = 0;
                        for (int note : ch)
                        {
                            const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                            for (int h = 1; h <= 3; ++h) s += std::sin (2 * dsp::kPiD * f * h * i / kr) / (h * 1.5);
                        }
                        v.push_back ((float) (0.08 * s));
                    }
            return v;
        };
        struct Case { const char* name; std::vector<std::vector<int>> prog; int key; bool minor; };
        std::vector<Case> cases {
            { "Sol majeur", { { 43, 55, 59, 62 }, { 48, 60, 64, 67 }, { 50, 62, 66, 69 }, { 43, 55, 59, 62 } }, 7, false },
            { "La mineur",  { { 45, 57, 60, 64 }, { 50, 62, 65, 69 }, { 52, 64, 68, 71 }, { 45, 57, 60, 64 } }, 9, true },
            { "Mib majeur", { { 39, 63, 67, 70 }, { 44, 68, 72, 75 }, { 46, 70, 74, 77 }, { 39, 63, 67, 70 } }, 3, false },
            { "Fa# mineur", { { 42, 66, 69, 73 }, { 47, 71, 74, 78 }, { 49, 73, 77, 80 }, { 42, 66, 69, 73 } }, 6, true },
        };
        for (auto& c : cases)
        {
            dsp::KeyFinder kf; kf.prepare (kr);
            auto sig = chordProg (c.prog);
            kf.push (sig.data(), (int) sig.size());
            auto r = kf.result();
            check (r.key == c.key && r.minor == c.minor, std::string ("Key Finder : ") + c.name + " -> " + std::to_string (r.key) + (r.minor ? "m" : "") + " (confiance " + std::to_string (r.confidence) + ")");
        }
    }
    std::printf ("=== %s (%d echec(s)) ===\n", failures == 0 ? "TOUT EST VERT" : "ECHEC", failures);
    return failures == 0 ? 0 : 1;
}
