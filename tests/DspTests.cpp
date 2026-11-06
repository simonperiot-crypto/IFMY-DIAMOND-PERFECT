// IFMY DIAMOND PERFECT — tests automatiques du DSP (lancés par GitHub Actions à chaque build)
// Si un test échoue, le build échoue : aucune version cassée n'arrive dans Logic.

#include "../Source/dsp/AutoTune.h"
#include "../Source/dsp/Effects.h"
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

static std::vector<float> runTuner (const std::vector<float>& in, double sr, const dsp::AutoTune::Params& p, bool& allFinite)
{
    dsp::AutoTune t;
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

    std::printf ("=== %s (%d echec(s)) ===\n", failures == 0 ? "TOUT EST VERT" : "ECHEC", failures);
    return failures == 0 ? 0 : 1;
}
