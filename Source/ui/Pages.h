#pragma once
#include "Widgets.h"
#include "../Params.h"
#include "../dsp/KeyFinder.h"

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
    // Panneau Key Finder (ouvert dans une bulle depuis la page TUNE)
    template <typename ProcessorType>
    class KeyFinderPanel : public juce::Component, public juce::Timer
    {
    public:
        explicit KeyFinderPanel (ProcessorType& proc) : pr (proc)
        {
            finder.prepare (pr.getKeyFinderRate());
            scratch.resize (8192);

            for (auto* b : { &voiceBtn, &instrBtn, &listenBtn, &applyBtn }) addAndMakeVisible (b);
            voiceBtn.setButtonText (T ("VOIX"));
            instrBtn.setButtonText (T ("INSTRU (side-chain)"));
            voiceBtn.setClickingTogglesState (true);
            instrBtn.setClickingTogglesState (true);
            voiceBtn.setRadioGroupId (77);
            instrBtn.setRadioGroupId (77);
            voiceBtn.setToggleState (pr.keyFinderSource.load() == 0, juce::dontSendNotification);
            instrBtn.setToggleState (pr.keyFinderSource.load() == 1, juce::dontSendNotification);
            instrBtn.setEnabled (! ProcessorType::isMidiVersion());
            voiceBtn.onClick = [this] { pr.keyFinderSource.store (0); restart(); };
            instrBtn.onClick = [this] { pr.keyFinderSource.store (1); restart(); };

            listenBtn.setButtonText (T ("ÉCOUTER"));
            listenBtn.onClick = [this] { if (listening) stop(); else start(); };
            applyBtn.setButtonText (T ("APPLIQUER"));
            applyBtn.setEnabled (false);
            applyBtn.onClick = [this] { apply(); };

            setSize (420, 320);
            startTimerHz (20);
        }

        ~KeyFinderPanel() override { pr.keyFinderActive.store (false); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (18);
            r.removeFromTop (30);
            auto src = r.removeFromTop (30);
            voiceBtn.setBounds (src.removeFromLeft (110));
            src.removeFromLeft (8);
            instrBtn.setBounds (src.removeFromLeft (180));
            auto bottom = r.removeFromBottom (38);
            listenBtn.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2 - 5));
            bottom.removeFromLeft (10);
            applyBtn.setBounds (bottom);
            area = r.reduced (0, 10);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme::bg1);
            auto r = getLocalBounds().reduced (18).toFloat();
            g.setColour (theme::text);
            g.setFont (theme::font (16.0f, true));
            g.drawText ("KEY FINDER", r.removeFromTop (24), juce::Justification::centredLeft);

            auto a = area.toFloat();
            const auto res = finder.result();
            const float progress = juce::jlimit (0.0f, 1.0f, finder.analysedFrames() / 40.0f);

            // Profil chromatique (12 barres)
            auto bars = a.removeFromBottom (54);
            const auto& prof = finder.profile();
            double mx = 0;
            for (double v : prof) mx = std::max (mx, v);
            static const char* names[12] = { "Do", "Do#", "Re", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" };
            const float bw = bars.getWidth() / 12.0f;
            for (int i = 0; i < 12; ++i)
            {
                auto col = juce::Rectangle<float> (bars.getX() + i * bw, bars.getY(), bw, bars.getHeight() - 14).reduced (3, 0);
                const float h = mx > 0 ? (float) (prof[(size_t) i] / mx) : 0.0f;
                g.setColour (juce::Colour (0x14ffffff));
                g.fillRoundedRectangle (col, 3);
                g.setColour (res.key == i ? theme::cyan : theme::violet.withAlpha (0.8f));
                g.fillRoundedRectangle (col.withTrimmedTop (col.getHeight() * (1.0f - h)), 3);
                g.setColour (theme::dim);
                g.setFont (theme::font (10.0f));
                g.drawText (T (names[i]), juce::Rectangle<float> (col.getX() - 4, bars.getBottom() - 13, col.getWidth() + 8, 12), juce::Justification::centred);
            }

            // Résultat
            a.removeFromBottom (6);
            if (res.key >= 0)
            {
                g.setGradientFill (theme::neon (a));
                g.setFont (theme::font (30.0f, true));
                g.drawText (keyName (res.key, res.minor), a.removeFromTop (40), juce::Justification::centred);
                g.setColour (theme::silver);
                g.setFont (theme::font (13.0f));
                g.drawText (T ("Confiance ") + juce::String (juce::roundToInt (res.confidence * 100)) + T (" %   ·   sinon : ") + keyName (res.secondKey, res.secondMinor),
                            a.removeFromTop (20), juce::Justification::centred);
            }
            else
            {
                g.setColour (theme::silver);
                g.setFont (theme::font (14.0f));
                g.drawText (listening ? T ("J'écoute… chante ou lance l'instru") : T ("Clique sur ÉCOUTER puis chante 15 à 30 s"),
                            a.removeFromTop (40), juce::Justification::centred);
            }

            auto bar = a.removeFromBottom (6).reduced (40, 0);
            g.setColour (juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (bar, 3);
            g.setGradientFill (theme::neon (bar));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3);
        }

        static juce::String keyName (int k, bool minor)
        {
            static const char* fr[12] = { "Do", "Do#", "Ré", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" };
            if (k < 0) return "-";
            return T (fr[k]) + (minor ? " mineur" : " majeur");
        }

    private:
        void start()
        {
            finder.reset();
            readPos = pr.keyFinderWritePos();
            pr.keyFinderActive.store (true);
            listening = true;
            listenBtn.setButtonText ("STOP");
            repaint();
        }

        void stop()
        {
            pr.keyFinderActive.store (false);
            listening = false;
            listenBtn.setButtonText (T ("ÉCOUTER"));
            repaint();
        }

        void restart() { if (listening) start(); }

        void apply()
        {
            const auto res = finder.result();
            if (res.key < 0) return;
            auto set = [this] (const char* id, int index)
            {
                if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (pr.apvts.getParameter (id)))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
                    prm->endChangeGesture();
                }
            };
            set (pid::key, res.key);
            set (pid::scale, res.minor ? 1 : 0); // Mineure naturelle / Majeure
            stop();
        }

        void timerCallback() override
        {
            if (listening)
            {
                int got;
                while ((got = pr.readKeyFinder (scratch.data(), (int) scratch.size(), readPos)) > 0)
                    finder.push (scratch.data(), got);
            }
            applyBtn.setEnabled (finder.result().key >= 0);
            repaint();
        }

        ProcessorType& pr;
        dsp::KeyFinder finder;
        std::vector<float> scratch;
        uint32_t readPos = 0;
        bool listening = false;
        juce::TextButton voiceBtn, instrBtn, listenBtn, applyBtn;
        juce::Rectangle<int> area;
    };

    //==========================================================================
    template <typename ProcessorType>
    class TunePage : public Page
    {
    public:
        explicit TunePage (ProcessorType& proc) : pr (proc), s (proc.apvts),
            key (s, pid::key), scale (s, pid::scale), range (s, pid::range), mode (s, pid::mode), engine (s, pid::engine),
            midiForce (s, pid::midiForce, "Notes MIDI"), formant (s, pid::formant, "Formants")
        {
            addAndMakeVisible (display);
            addAndMakeVisible (keyboard);
            for (auto* c : { &key, &scale, &range, &mode, &engine }) addAndMakeVisible (c);
            addAndMakeVisible (formant);
            addAndMakeVisible (keyBtn);
            if (ProcessorType::isMidiVersion()) addAndMakeVisible (midiForce);

            key.setTooltip (T ("Tonalité"));
            scale.setTooltip (T ("Gamme"));
            range.setTooltip (T ("Plage vocale : règle la détection (et la latence du moteur Studio)"));
            mode.setTooltip (T ("Auto = moderne et naturel · Classic = correction vintage"));
            engine.setTooltip (T ("Studio = meilleure qualité, formants conservés · Live = faible latence pour enregistrer"));
            formant.setTooltip (T ("Conserver le timbre de la voix quand elle est transposée (moteur Studio)"));

            keyBtn.setButtonText (T ("◆ KEY FINDER"));
            keyBtn.setTooltip (T ("Détecter la tonalité en écoutant ta voix ou l'instru"));
            keyBtn.onClick = [this]
            {
                auto panel = std::make_unique<KeyFinderPanel<ProcessorType>> (pr);
                panel->setLookAndFeel (&getLookAndFeel());
                juce::CallOutBox::launchAsynchronously (std::move (panel), keyBtn.getScreenBounds(), nullptr);
            };

            retune    = &knob (s, pid::retune,    "VITESSE");
            amount    = &knob (s, pid::amount,    "INTENSITÉ");
            humanize  = &knob (s, pid::humanize,  "HUMANIZE");
            flex      = &knob (s, pid::flex,      "FLEX-TUNE");
            transpose = &knob (s, pid::transpose, "TRANSPOSE");
            throat    = &knob (s, pid::throat,    "GORGE");
            ref       = &knob (s, pid::refHz,     "RÉFÉRENCE LA", true);
            vDepth    = &knob (s, pid::vibDepth,  "VIBRATO", true);
            vRate     = &knob (s, pid::vibRate,   "VIT. VIBRATO", true);
            vDelay    = &knob (s, pid::vibDelay,  "DÉLAI VIBRATO", true);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            display.setBounds (r.removeFromTop (expert ? 172 : 238));
            r.removeFromTop (12);

            auto row = r.removeFromTop (84);
            auto left = row.removeFromLeft (330);
            auto l1 = left.removeFromTop (40).reduced (0, 4);
            key.setBounds (l1.removeFromLeft (110)); l1.removeFromLeft (8);
            scale.setBounds (l1);
            auto l2 = left.removeFromTop (40).reduced (0, 4);
            range.setBounds (l2.removeFromLeft (134)); l2.removeFromLeft (8);
            mode.setBounds (l2.removeFromLeft (90)); l2.removeFromLeft (8);
            if (ProcessorType::isMidiVersion()) midiForce.setBounds (l2);

            auto right = row.removeFromRight (226);
            auto r1 = right.removeFromTop (40).reduced (0, 4);
            engine.setBounds (r1);
            auto r2 = right.removeFromTop (40).reduced (0, 4);
            keyBtn.setBounds (r2.removeFromLeft (112)); r2.removeFromLeft (8);
            formant.setBounds (r2);

            keyboard.setBounds (row.reduced (12, 0));

            r.removeFromTop (10);
            if (expert)
            {
                auto top = r.removeFromTop (r.getHeight() / 2);
                r.removeFromTop (4);
                layoutRow (top, { retune, amount, humanize, flex, transpose, throat }, 120);
                layoutRow (r, { ref, vDepth, vRate, vDelay }, 120);
            }
            else
            {
                layoutRow (r, { retune, amount, humanize, flex, transpose, throat }, 120);
            }
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
            formant.setEnabled (*s.getRawParameterValue (pid::engine) < 0.5f);
        }

    private:
        ProcessorType& pr;
        APVTS& s;
        PitchDisplay display;
        ScaleKeyboard keyboard;
        Choice key, scale, range, mode, engine;
        Toggle midiForce, formant;
        juce::TextButton keyBtn;
        Knob *retune, *amount, *humanize, *flex, *transpose, *throat, *ref, *vDepth, *vRate, *vDelay;
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
            g.drawText (T ("IFMY DIAMOND PERFECT · Phase 2"), t.removeFromTop (24), juce::Justification::centredLeft);
            g.setFont (theme::font (13.5f));
            g.setColour (theme::silver);
            const double latMs = pr.getSampleRate() > 0 ? 1000.0 * pr.getLatencySamples() / pr.getSampleRate() : 10.0;
            juce::StringArray lines {
                T ("Latence déclarée à Logic : ") + juce::String (latMs, 1) + T (" ms (compensée automatiquement)"),
                T ("Moteur TUNE : Studio = formants conservés · Live ≈ 3 ms pour enregistrer"),
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
