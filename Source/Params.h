#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Scales.h"
#include "dsp/AutoTune.h"

// ============================================================================
//  IDs de paramètres FIXES.
//  Règle : on n'en renomme ni n'en supprime JAMAIS un (projets Logic + presets).
//  Pour ajouter un paramètre dans une phase suivante : nouvel ID + version = numéro de phase.
// ============================================================================
namespace pid
{
    // Master
    inline constexpr const char* bypass    = "bypass";
    inline constexpr const char* inGain    = "in_gain";
    inline constexpr const char* outGain   = "out_gain";
    inline constexpr const char* safety    = "safety_clip";

    // TUNE
    inline constexpr const char* tuneOn    = "tune_on";
    inline constexpr const char* key       = "tune_key";
    inline constexpr const char* scale     = "tune_scale";
    inline constexpr const char* retune    = "tune_retune";
    inline constexpr const char* amount    = "tune_amount";
    inline constexpr const char* humanize  = "tune_humanize";
    inline constexpr const char* transpose = "tune_transpose";
    inline constexpr const char* refHz     = "tune_ref";
    inline constexpr const char* midiForce = "tune_midi";
    // TUNE — Phase 2
    inline constexpr const char* engine    = "tune_engine";
    inline constexpr const char* mode      = "tune_mode";
    inline constexpr const char* range     = "tune_range";
    inline constexpr const char* flex      = "tune_flex";
    inline constexpr const char* formant   = "tune_formant";
    inline constexpr const char* throat    = "tune_throat";
    inline constexpr const char* vibDepth  = "vib_depth";
    inline constexpr const char* vibRate   = "vib_rate";
    inline constexpr const char* vibDelay  = "vib_delay";

    // DYNAMICS
    inline constexpr const char* dynOn     = "dyn_on";
    inline constexpr const char* gateOn    = "gate_on";
    inline constexpr const char* gateThr   = "gate_thr";
    inline constexpr const char* dsOn      = "ds_on";
    inline constexpr const char* dsFreq    = "ds_freq";
    inline constexpr const char* dsThr     = "ds_thr";
    inline constexpr const char* cmpOn     = "cmp_on";
    inline constexpr const char* cmpThr    = "cmp_thr";
    inline constexpr const char* cmpRatio  = "cmp_ratio";
    inline constexpr const char* cmpAtt    = "cmp_att";
    inline constexpr const char* cmpRel    = "cmp_rel";
    inline constexpr const char* cmpGain   = "cmp_gain";
    inline constexpr const char* satOn     = "sat_on";
    inline constexpr const char* satDrive  = "sat_drive";
    inline constexpr const char* satMix    = "sat_mix";

    // EQ
    inline constexpr const char* eqOn      = "eq_on";
    inline constexpr const char* eqHpf     = "eq_hpf";
    inline constexpr const char* eqMud     = "eq_mud";
    inline constexpr const char* eqPres    = "eq_pres";
    inline constexpr const char* eqAir     = "eq_air";

    // FX
    inline constexpr const char* fxOn      = "fx_on";
    inline constexpr const char* dblOn     = "dbl_on";
    inline constexpr const char* dblMix    = "dbl_mix";
    inline constexpr const char* dblWidth  = "dbl_width";
    inline constexpr const char* dlyOn     = "dly_on";
    inline constexpr const char* dlyDiv    = "dly_div";
    inline constexpr const char* dlyFb     = "dly_fb";
    inline constexpr const char* dlyTone   = "dly_tone";
    inline constexpr const char* dlyMix    = "dly_mix";
    inline constexpr const char* dlyPing   = "dly_ping";
    inline constexpr const char* revOn     = "rev_on";
    inline constexpr const char* revSize   = "rev_size";
    inline constexpr const char* revDamp   = "rev_damp";
    inline constexpr const char* revWidth  = "rev_width";
    inline constexpr const char* revMix    = "rev_mix";

    // Paramètres que les presets ne touchent pas (réglages de la session du chanteur)
    inline bool isSessionParam (const juce::String& id)
    {
        return id == bypass || id == inGain || id == outGain || id == safety || id == engine;
    }

    // Réglages liés au chanteur / au morceau, gardés quand on change de preset (option « garder ma tonalité »)
    inline bool isKeyParam (const juce::String& id) { return id == key || id == scale || id == range; }
}

namespace params
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;

    inline juce::ParameterID id (const char* s, int version = 1) { return { s, version }; }

    inline std::unique_ptr<APF> f (const char* i, const char* name, float lo, float hi, float def, const char* unit,
                                   int decimals = 1, float skewCentre = -1.0f)
    {
        juce::NormalisableRange<float> r (lo, hi);
        if (skewCentre > lo && skewCentre < hi) r.setSkewForCentre (skewCentre);
        juce::String u (unit);
        auto attrs = juce::AudioParameterFloatAttributes()
                        .withLabel (u)
                        .withStringFromValueFunction ([u, decimals] (float v, int)
                        {
                            if (std::abs (v) < 0.05f && decimals > 0) v = 0.0f;
                            if (u == "Hz" && v >= 1000.0f) return juce::String (v / 1000.0f, 1) + " kHz";
                            return juce::String (v, decimals) + (u.isEmpty() ? "" : " " + u);
                        });
        return std::make_unique<APF> (id (i), name, r, def, attrs);
    }

    // Paramètre ajouté en Phase 2
    inline std::unique_ptr<APF> f2 (const char* i, const char* name, float lo, float hi, float def, const char* unit, int decimals = 1)
    {
        auto p = f (i, name, lo, hi, def, unit, decimals);
        juce::String u (unit);
        auto attrs = juce::AudioParameterFloatAttributes().withLabel (u)
                        .withStringFromValueFunction ([u, decimals] (float v, int) { return juce::String (v, decimals) + (u.isEmpty() ? "" : " " + u); });
        return std::make_unique<APF> (id (i, 2), name, p->getNormalisableRange(), def, attrs);
    }

    inline std::unique_ptr<APB> b (const char* i, const char* name, bool def)
    {
        return std::make_unique<APB> (id (i), name, def);
    }

    inline const juce::StringArray& delayDivisions()
    {
        static const juce::StringArray d { "1/2", "1/4 .", "1/4", "1/8 .", "1/4 T", "1/8", "1/8 T", "1/16" };
        return d;
    }

    inline constexpr float kDivBeats[] = { 2.0f, 1.5f, 1.0f, 0.75f, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;

        juce::StringArray keys, scales;
        for (auto* k : dsp::kKeyNamesLong) keys.add (k);
        for (auto& s : dsp::kScales) scales.add (s.name);

        // Master
        v.push_back (b (pid::bypass, "Bypass", false));
        v.push_back (f (pid::inGain,  "Input",  -24, 24, 0, "dB"));
        v.push_back (f (pid::outGain, "Output", -24, 24, 0, "dB"));
        v.push_back (b (pid::safety,  "Safety Clip", true));

        // TUNE
        v.push_back (b (pid::tuneOn,    "Tune On", true));
        v.push_back (std::make_unique<APC> (id (pid::key),   "Key",   keys,   0));
        v.push_back (std::make_unique<APC> (id (pid::scale), "Scale", scales, 0));
        v.push_back (f (pid::retune,    "Retune Speed", 0, 400, 40, "ms", 0, 60));
        v.push_back (f (pid::amount,    "Intensity", 0, 100, 100, "%", 0));
        v.push_back (f (pid::humanize,  "Humanize", 0, 100, 0, "%", 0));
        v.push_back (f (pid::transpose, "Transpose", -12, 12, 0, "st", 0));
        v.push_back (f (pid::refHz,     "Reference A4", 430, 450, 440, "Hz", 1));
        v.push_back (b (pid::midiForce, "MIDI Notes Force", true));

        // DYNAMICS
        v.push_back (b (pid::dynOn,    "Dynamics On", true));
        v.push_back (b (pid::gateOn,   "Gate On", false));
        v.push_back (f (pid::gateThr,  "Gate Threshold", -80, -20, -55, "dB"));
        v.push_back (b (pid::dsOn,     "De-Esser On", true));
        v.push_back (f (pid::dsFreq,   "De-Ess Freq", 3000, 10000, 6500, "Hz", 0));
        v.push_back (f (pid::dsThr,    "De-Ess Threshold", -50, 0, -26, "dB"));
        v.push_back (b (pid::cmpOn,    "Compressor On", true));
        v.push_back (f (pid::cmpThr,   "Comp Threshold", -50, 0, -18, "dB"));
        v.push_back (f (pid::cmpRatio, "Comp Ratio", 1, 20, 3, ":1", 1, 4));
        v.push_back (f (pid::cmpAtt,   "Comp Attack", 0.3f, 50, 5, "ms", 1, 6));
        v.push_back (f (pid::cmpRel,   "Comp Release", 20, 600, 120, "ms", 0, 120));
        v.push_back (f (pid::cmpGain,  "Comp Makeup", 0, 24, 3, "dB"));
        v.push_back (b (pid::satOn,    "Saturation On", false));
        v.push_back (f (pid::satDrive, "Drive", 0, 24, 6, "dB"));
        v.push_back (f (pid::satMix,   "Saturation Mix", 0, 100, 40, "%", 0));

        // EQ
        v.push_back (b (pid::eqOn,   "EQ On", true));
        v.push_back (f (pid::eqHpf,  "Low Cut", 20, 400, 80, "Hz", 0, 100));
        v.push_back (f (pid::eqMud,  "Mud (300 Hz)", -12, 6, 0, "dB"));
        v.push_back (f (pid::eqPres, "Presence (3.5 kHz)", -6, 12, 0, "dB"));
        v.push_back (f (pid::eqAir,  "Air (12 kHz)", -6, 12, 0, "dB"));

        // FX
        v.push_back (b (pid::fxOn,     "FX On", true));
        v.push_back (b (pid::dblOn,    "Doubler On", false));
        v.push_back (f (pid::dblMix,   "Doubler Mix", 0, 100, 30, "%", 0));
        v.push_back (f (pid::dblWidth, "Doubler Width", 0, 100, 80, "%", 0));
        v.push_back (b (pid::dlyOn,    "Delay On", false));
        v.push_back (std::make_unique<APC> (id (pid::dlyDiv), "Delay Time", delayDivisions(), 5));
        v.push_back (f (pid::dlyFb,    "Delay Feedback", 0, 90, 30, "%", 0));
        v.push_back (f (pid::dlyTone,  "Delay Tone", 800, 16000, 5000, "Hz", 0, 4000));
        v.push_back (f (pid::dlyMix,   "Delay Mix", 0, 100, 15, "%", 0));
        v.push_back (b (pid::dlyPing,  "Ping-Pong", true));
        v.push_back (b (pid::revOn,    "Reverb On", true));
        v.push_back (f (pid::revSize,  "Reverb Size", 0, 100, 55, "%", 0));
        v.push_back (f (pid::revDamp,  "Reverb Damping", 0, 100, 45, "%", 0));
        v.push_back (f (pid::revWidth, "Reverb Width", 0, 100, 90, "%", 0));
        v.push_back (f (pid::revMix,   "Reverb Mix", 0, 100, 12, "%", 0));

        // ---- Phase 2 (ajoutés à la fin, version 2 : compatibilité des projets v0.1) ----
        juce::StringArray ranges;
        for (auto& r : dsp::kRanges) ranges.add (juce::String (r.name).replace ("Tenor", juce::String::fromUTF8 ("Ténor")));
        v.push_back (std::make_unique<APC> (id (pid::engine, 2), "Engine",
                        juce::StringArray { "Studio (formants)", juce::String::fromUTF8 ("Live (faible latence)") }, 0));
        v.push_back (std::make_unique<APC> (id (pid::mode, 2),  "Tune Mode", juce::StringArray { "Auto", "Classic" }, 0));
        v.push_back (std::make_unique<APC> (id (pid::range, 2), "Voice Range", ranges, dsp::kDefaultRange));
        v.push_back (f2 (pid::flex,     "Flex-Tune", 0, 100, 0, "%", 0));
        v.push_back (std::make_unique<APB> (id (pid::formant, 2), "Formant Preserve", true));
        v.push_back (f2 (pid::throat,   "Throat", 60, 140, 100, "%", 0));
        v.push_back (f2 (pid::vibDepth, "Vibrato Depth", 0, 100, 0, "ct", 0));
        v.push_back (f2 (pid::vibRate,  "Vibrato Rate", 2, 9, 5.5f, "Hz", 1));
        v.push_back (f2 (pid::vibDelay, "Vibrato Delay", 0, 1000, 250, "ms", 0));

        return { v.begin(), v.end() };
    }
}
