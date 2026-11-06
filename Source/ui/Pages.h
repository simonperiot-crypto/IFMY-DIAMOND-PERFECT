#pragma once
#include "Widgets.h"
#include "../Params.h"

namespace ui
{
    // Base commune aux pages de modules
    class Page : public juce::Component
    {
    public:
        virtual void setExpert (bool e)
        {
            expert = e;
            for (auto* k : knobs) k->setVisible (! k->isExpertOnly() || expert);
            resized();
        }
        virtual void tick() {}

    protected:
        Knob& knob (APVTS& s, const char* id, const char* title, bool expertOnly = false)
        {
            auto* k = owned.add (new Knob (s, id, T (title), expertOnly));
            knobs.add (k);
            addChildComponent (k);
            k->setVisible (! expertOnly);
            return *k;
        }

        Toggle& toggle (APVTS& s, const char* id, const char* title = "")
        {
            auto* t = toggles.add (new Toggle (s, id, T (title)));
            return *t;
        }

        Section& section (const char* title, juce::ToggleButton* power)
        {
            auto* sec = sections.add (new Section (T (title), power));
            addAndMakeVisible (sec);
            return *sec;
        }

        // Les boutons d'une section sont des enfants de la page, posés dans la zone de la section
        juce::Rectangle<int> inside (Section& s) const { return s.content() + s.getPosition(); }

        void bringKnobsToFront() { for (auto* k : knobs) k->toFront (false); }

        bool expert = false;
        juce::OwnedArray<Knob> owned;
        juce::Array<Knob*> knobs;
        juce::OwnedArray<Toggle> toggles;
        juce::OwnedArray<Section> sections;
    };

    //==========================================================================
    template <typename ProcessorType>
    class TunePage : public Page
    {
    public:
        explicit TunePage (ProcessorType& proc) : pr (proc), s (proc.apvts),
            key (s, pid::key), scale (s, pid::scale), midiForce (s, pid::midiForce, "Notes MIDI")
        {
            addAndMakeVisible (display);
            addAndMakeVisible (keyboard);
            addAndMakeVisible (key);
            addAndMakeVisible (scale);
            if (ProcessorType::isMidiVersion()) addAndMakeVisible (midiForce);

            retune    = &knob (s, pid::retune,    "VITESSE");
            amount    = &knob (s, pid::amount,    "INTENSITÉ");
            humanize  = &knob (s, pid::humanize,  "HUMANIZE");
            transpose = &knob (s, pid::transpose, "TRANSPOSE");
            ref       = &knob (s, pid::refHz,     "RÉFÉRENCE LA", true);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            display.setBounds (r.removeFromTop (r.getHeight() * 46 / 100));
            r.removeFromTop (14);

            auto row = r.removeFromTop (86);
            auto left = row.removeFromLeft (300);
            keyboard.setBounds (row.reduced (12, 0));
            auto l1 = left.removeFromTop (40);
            key.setBounds (l1.removeFromLeft (110).reduced (0, 4));
            l1.removeFromLeft (8);
            scale.setBounds (l1.reduced (0, 4));
            if (ProcessorType::isMidiVersion()) midiForce.setBounds (left.removeFromTop (34).reduced (0, 4).withWidth (160));

            r.removeFromTop (12);
            layoutRow (r, { retune, amount, humanize, transpose, ref }, 130);
        }

        void tick() override
        {
            auto& t = pr.tuner;
            display.push (t.uiDetected.load(), t.uiOutput.load(), t.uiTarget.load());
            const uint16_t m = t.currentMask();
            display.setMask (m);
            const float tg = t.uiTarget.load();
            const bool midiNow = ProcessorType::isMidiVersion() && pr.midiNotesHeld.load() > 0 && *s.getRawParameterValue (pid::midiForce) > 0.5f;
            keyboard.setState (m, tg > 0 && t.uiDetected.load() > 0 ? ((int) std::lround (tg)) % 12 : -1, midiNow);
        }

    private:
        ProcessorType& pr;
        APVTS& s;
        PitchDisplay display;
        ScaleKeyboard keyboard;
        Choice key, scale;
        Toggle midiForce;
        Knob *retune, *amount, *humanize, *transpose, *ref;
    };

    //==========================================================================
    class DynamicsPage : public Page
    {
    public:
        template <typename P>
        explicit DynamicsPage (P& proc)
        {
            auto& s = proc.apvts;
            gate = &section ("GATE", &toggle (s, pid::gateOn));
            ds   = &section ("DE-ESSER", &toggle (s, pid::dsOn));
            cmp  = &section ("COMPRESSEUR", &toggle (s, pid::cmpOn));
            sat  = &section ("SATURATION", &toggle (s, pid::satOn));

            gThr  = &knob (s, pid::gateThr, "SEUIL");
            dFreq = &knob (s, pid::dsFreq, "FRÉQUENCE", true);
            dThr  = &knob (s, pid::dsThr, "SEUIL");
            cThr  = &knob (s, pid::cmpThr, "SEUIL");
            cRat  = &knob (s, pid::cmpRatio, "RATIO");
            cAtt  = &knob (s, pid::cmpAtt, "ATTAQUE", true);
            cRel  = &knob (s, pid::cmpRel, "RELEASE", true);
            cGain = &knob (s, pid::cmpGain, "GAIN");
            sDrv  = &knob (s, pid::satDrive, "DRIVE");
            sMix  = &knob (s, pid::satMix, "MIX");
            bringKnobsToFront();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto top = r.removeFromTop (r.getHeight() / 2 - 6);
            r.removeFromTop (12);
            gate->setBounds (top.removeFromLeft (top.getWidth() * 30 / 100)); top.removeFromLeft (12);
            ds->setBounds (top);
            cmp->setBounds (r.removeFromLeft (r.getWidth() * 66 / 100)); r.removeFromLeft (12);
            sat->setBounds (r);

            layoutRow (inside (*gate), { gThr }, 120);
            layoutRow (inside (*ds),   { dFreq, dThr }, 120);
            layoutRow (inside (*cmp),  { cThr, cRat, cAtt, cRel, cGain }, 120);
            layoutRow (inside (*sat),  { sDrv, sMix }, 120);
        }

    private:
        Section *gate, *ds, *cmp, *sat;
        Knob *gThr, *dFreq, *dThr, *cThr, *cRat, *cAtt, *cRel, *cGain, *sDrv, *sMix;
    };

    //==========================================================================
    class EqPage : public Page
    {
    public:
        template <typename P>
        explicit EqPage (P& proc) : s (proc.apvts)
        {
            addAndMakeVisible (curve);
            sec  = &section ("EQ VOCAL", nullptr);
            hpf  = &knob (s, pid::eqHpf, "COUPE-BAS");
            mud  = &knob (s, pid::eqMud, "BOUE 300 Hz");
            pres = &knob (s, pid::eqPres, "PRÉSENCE");
            air  = &knob (s, pid::eqAir, "AIR");
            bringKnobsToFront();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            curve.setBounds (r.removeFromTop (r.getHeight() * 52 / 100));
            r.removeFromTop (12);
            sec->setBounds (r);
            layoutRow (inside (*sec), { hpf, mud, pres, air }, 140);
        }

        void tick() override
        {
            curve.setValues (*s.getRawParameterValue (pid::eqHpf), *s.getRawParameterValue (pid::eqMud),
                             *s.getRawParameterValue (pid::eqPres), *s.getRawParameterValue (pid::eqAir),
                             *s.getRawParameterValue (pid::eqOn) > 0.5f);
        }

    private:
        APVTS& s;
        EqCurve curve;
        Section* sec;
        Knob *hpf, *mud, *pres, *air;
    };

    //==========================================================================
    class FxPage : public Page
    {
    public:
        template <typename P>
        explicit FxPage (P& proc) : div (proc.apvts, pid::dlyDiv), ping (proc.apvts, pid::dlyPing, "Ping-pong")
        {
            auto& s = proc.apvts;
            dblSec = &section ("DOUBLEUR", &toggle (s, pid::dblOn));
            dlySec = &section ("DELAY", &toggle (s, pid::dlyOn));
            revSec = &section ("REVERB", &toggle (s, pid::revOn));

            dMix   = &knob (s, pid::dblMix, "MIX");
            dWid   = &knob (s, pid::dblWidth, "LARGEUR", true);
            yFb    = &knob (s, pid::dlyFb, "FEEDBACK");
            yTone  = &knob (s, pid::dlyTone, "TONALITÉ", true);
            yMix   = &knob (s, pid::dlyMix, "MIX");
            rSize  = &knob (s, pid::revSize, "TAILLE");
            rDamp  = &knob (s, pid::revDamp, "AMORTI", true);
            rWid   = &knob (s, pid::revWidth, "LARGEUR", true);
            rMix   = &knob (s, pid::revMix, "MIX");
            addAndMakeVisible (div);
            addAndMakeVisible (ping);
            bringKnobsToFront();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto top = r.removeFromTop (r.getHeight() / 2 - 6);
            r.removeFromTop (12);
            dblSec->setBounds (top.removeFromLeft (top.getWidth() * 36 / 100)); top.removeFromLeft (12);
            dlySec->setBounds (top);
            revSec->setBounds (r);

            layoutRow (inside (*dblSec), { dMix, dWid }, 120);
            auto d = inside (*dlySec);
            auto side = d.removeFromLeft (130);
            div.setBounds (side.removeFromTop (34).reduced (0, 2));
            side.removeFromTop (10);
            ping.setBounds (side.removeFromTop (28));
            layoutRow (d, { yFb, yTone, yMix }, 120);
            layoutRow (inside (*revSec), { rSize, rDamp, rWid, rMix }, 130);
        }

    private:
        Section *dblSec, *dlySec, *revSec;
        Knob *dMix, *dWid, *yFb, *yTone, *yMix, *rSize, *rDamp, *rWid, *rMix;
        Choice div;
        Toggle ping;
    };

    //==========================================================================
    template <typename ProcessorType>
    class MasterPage : public Page
    {
    public:
        explicit MasterPage (ProcessorType& proc) : pr (proc), safety (proc.apvts, pid::safety, "Anti-saturation")
        {
            auto& s = proc.apvts;
            sec = &section ("MASTER", nullptr);
            in  = &knob (s, pid::inGain, "ENTRÉE");
            out = &knob (s, pid::outGain, "SORTIE");
            addAndMakeVisible (safety);
            bringKnobsToFront();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            sec->setBounds (r.removeFromTop (240));
            auto c = inside (*sec);
            safety.setBounds (c.removeFromBottom (30).withSizeKeepingCentre (200, 28));
            layoutRow (c, { in, out }, 150);
            info = r.reduced (4, 16);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = info.toFloat();
            theme::glassPanel (g, r, 14.0f);
            auto t = r.reduced (20, 16);
            g.setColour (theme::text);
            g.setFont (theme::font (15.0f, true));
            g.drawText (T ("IFMY DIAMOND PERFECT · Phase 1"), t.removeFromTop (24), juce::Justification::centredLeft);
            g.setFont (theme::font (13.5f));
            g.setColour (theme::silver);
            const double latMs = pr.getSampleRate() > 0 ? 1000.0 * pr.getLatencySamples() / pr.getSampleRate() : 10.0;
            juce::StringArray lines {
                T ("Latence déclarée à Logic : ") + juce::String (latMs, 1) + " ms (compensée automatiquement)",
                T (ProcessorType::isMidiVersion() ? "Version MIDI : joue des notes au clavier pour forcer la justesse"
                                                 : "Version effet : pour le clavier MIDI, utilise « IFMY Diamond Perfect MIDI »"),
                T ("Anti-saturation : protège tes enceintes et ton mix des pics au-dessus de 0 dB"),
                T ("Bypass sans clic · Toutes fréquences 44,1 à 192 kHz · Tous buffers") };
            for (auto& l : lines) g.drawText (l, t.removeFromTop (24), juce::Justification::centredLeft);
        }

    private:
        ProcessorType& pr;
        Section* sec;
        Knob *in, *out;
        Toggle safety;
        juce::Rectangle<int> info;
    };
}
