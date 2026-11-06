#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include <array>
#include <complex>
#include "../dsp/DSP.h"
#include "../dsp/Scales.h"

namespace ui
{
    using APVTS = juce::AudioProcessorValueTreeState;
    using theme::T;

    //==========================================================================
    // Bouton rotatif + nom + valeur. Double-clic = valeur par défaut.
    class Knob : public juce::Component, private juce::Slider::Listener
    {
    public:
        Knob (APVTS& s, const juce::String& paramId, const juce::String& title, bool expertOnly = false)
            : name (title), expert (expertOnly), param (s.getParameter (paramId))
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            slider.setPopupDisplayEnabled (false, false, nullptr);
            addAndMakeVisible (slider);
            attachment = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);
            if (param != nullptr)
                slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            slider.addListener (this);
        }

        ~Knob() override { slider.removeListener (this); }

        bool isExpertOnly() const { return expert; }

        void resized() override
        {
            auto r = getLocalBounds();
            r.removeFromTop (16);
            r.removeFromBottom (18);
            slider.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (theme::dim);
            g.setFont (theme::font (11.5f, true));
            g.drawText (name, r.removeFromTop (16), juce::Justification::centred);
            g.setColour (isEnabled() ? theme::text : theme::faint);
            g.setFont (theme::font (13.0f, true));
            g.drawText (param != nullptr ? param->getCurrentValueAsText() : juce::String(),
                        r.removeFromBottom (18), juce::Justification::centred);
        }

        void enablementChanged() override { slider.setEnabled (isEnabled()); repaint(); }

    private:
        void sliderValueChanged (juce::Slider*) override { repaint(); }

        juce::String name;
        bool expert;
        juce::RangedAudioParameter* param;
        juce::Slider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    //==========================================================================
    class Toggle : public juce::ToggleButton
    {
    public:
        Toggle (APVTS& s, const juce::String& paramId, const juce::String& title) : juce::ToggleButton (title)
        {
            attachment = std::make_unique<APVTS::ButtonAttachment> (s, paramId, *this);
        }
    private:
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    //==========================================================================
    class Choice : public juce::ComboBox
    {
    public:
        Choice (APVTS& s, const juce::String& paramId)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
                addItemList (c->choices, 1);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, paramId, *this);
        }
    private:
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    //==========================================================================
    // Vumètre vertical (crête avec retombée + maintien)
    class Meter : public juce::Component
    {
    public:
        explicit Meter (juce::String label, bool reduction = false) : title (std::move (label)), gr (reduction) {}

        void setLevel (float v)
        {
            if (gr) level = v;                              // dB de réduction
            else    level = juce::jmax (v, level * 0.86f);  // linéaire
            if (level > peakHold) { peakHold = level; holdTicks = 40; }
            else if (--holdTicks < 0) peakHold *= gr ? 0.9f : 0.97f;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (theme::dim);
            g.setFont (theme::font (11.0f, true));
            g.drawText (title, r.removeFromBottom (16), juce::Justification::centred);
            auto bar = r.withSizeKeepingCentre (juce::jmin (14.0f, r.getWidth()), r.getHeight());
            g.setColour (juce::Colour (0x14ffffff));
            g.fillRoundedRectangle (bar, 4);

            float frac;
            if (gr) frac = juce::jlimit (0.0f, 1.0f, level / 24.0f);
            else    frac = juce::jlimit (0.0f, 1.0f, (dsp::gainToDb (level) + 60.0f) / 66.0f);
            if (frac > 0.001f)
            {
                auto fill = gr ? bar.withHeight (bar.getHeight() * frac)
                               : bar.withTrimmedTop (bar.getHeight() * (1.0f - frac));
                if (gr) g.setColour (theme::warn);
                else
                {
                    juce::ColourGradient cg (theme::hot, bar.getX(), bar.getY(), theme::cyan, bar.getX(), bar.getBottom(), false);
                    cg.addColour (0.12, theme::warn);
                    cg.addColour (0.3, theme::violet);
                    g.setGradientFill (cg);
                }
                g.fillRoundedRectangle (fill, 4);
            }

            if (! gr)
            {
                const float hf = juce::jlimit (0.0f, 1.0f, (dsp::gainToDb (peakHold) + 60.0f) / 66.0f);
                if (hf > 0.01f)
                {
                    g.setColour (peakHold > 0.99f ? theme::hot : juce::Colours::white);
                    g.fillRect (bar.getX(), bar.getBottom() - bar.getHeight() * hf, bar.getWidth(), 2.0f);
                }
            }
        }

    private:
        juce::String title;
        bool gr;
        float level = 0, peakHold = 0;
        int holdTicks = 0;
    };

    //==========================================================================
    // Courbe de hauteur en temps réel : voix détectée (argent) + voix corrigée (néon)
    class PitchDisplay : public juce::Component
    {
    public:
        static constexpr int kLen = 300;

        void push (float detected, float output, float target)
        {
            det[(size_t) pos] = detected;
            out[(size_t) pos] = output;
            pos = (pos + 1) % kLen;
            lastTarget = target;
            if (detected > 0) centre += (detected - centre) * 0.03f;
            repaint();
        }

        void setMask (uint16_t m) { mask = m; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            theme::glassPanel (g, r, 14.0f);
            auto plot = r.reduced (12.0f).withTrimmedLeft (34.0f);

            const float span = 14.0f; // demi-tons visibles
            const float lo = centre - span * 0.5f;
            auto yOf = [&] (float midi) { return plot.getBottom() - (midi - lo) / span * plot.getHeight(); };

            // grille : notes de la gamme en surbrillance
            static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            for (int n = (int) std::floor (lo); n <= (int) std::ceil (lo + span); ++n)
            {
                const float y = yOf ((float) n);
                if (y < plot.getY() - 1 || y > plot.getBottom() + 1) continue;
                const bool in = (mask >> (((n % 12) + 12) % 12)) & 1u;
                const bool isTarget = (int) std::lround (lastTarget) == n && lastTarget > 0;
                g.setColour (isTarget ? theme::cyan.withAlpha (0.35f) : juce::Colour (in ? 0x22ffffff : 0x0affffff));
                g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
                g.setColour (isTarget ? theme::cyan : (in ? theme::silver : theme::faint));
                g.setFont (theme::font (11.0f, in));
                g.drawText (juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1),
                            juce::Rectangle<float> (r.getX() + 8, y - 7, 36, 14), juce::Justification::centredLeft);
            }

            auto curve = [&] (const std::array<float, kLen>& data, juce::Colour c, float width, bool glow)
            {
                juce::Path p;
                bool open = false;
                for (int i = 0; i < kLen; ++i)
                {
                    const float v = data[(size_t) ((pos + i) % kLen)];
                    const float x = plot.getX() + plot.getWidth() * (float) i / (kLen - 1);
                    if (v <= 0) { open = false; continue; }
                    const float y = juce::jlimit (plot.getY(), plot.getBottom(), yOf (v));
                    if (! open) { p.startNewSubPath (x, y); open = true; }
                    else p.lineTo (x, y);
                }
                if (glow)
                {
                    g.setColour (c.withAlpha (0.18f));
                    g.strokePath (p, juce::PathStrokeType (width * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };

            curve (det, theme::silver.withAlpha (0.55f), 1.5f, false);
            curve (out, theme::cyan, 2.4f, true);

            g.setFont (theme::font (11.0f, true));
            g.setColour (theme::silver.withAlpha (0.7f));
            g.drawText (T ("VOIX"), r.reduced (14).removeFromTop (14).removeFromRight (150), juce::Justification::left);
            g.setColour (theme::cyan);
            g.drawText (T ("CORRIGÉE"), r.reduced (14).removeFromTop (14).removeFromRight (100), juce::Justification::left);
        }

    private:
        std::array<float, kLen> det {}, out {};
        int pos = 0;
        float centre = 60.0f, lastTarget = -1;
        uint16_t mask = 0x0AB5;
    };

    //==========================================================================
    // Clavier d'une octave : notes de la gamme allumées, note cible en néon
    class ScaleKeyboard : public juce::Component
    {
    public:
        void setState (uint16_t scaleMask, int targetPc, bool midiMode)
        {
            if (scaleMask == mask && targetPc == target && midiMode == midi) return;
            mask = scaleMask; target = targetPc; midi = midiMode;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            static const int whites[7] = { 0, 2, 4, 5, 7, 9, 11 };
            static const int blacks[5] = { 1, 3, 6, 8, 10 };
            static const float blackPos[5] = { 1, 2, 4, 5, 6 };
            static const char* fr[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
            const float ww = r.getWidth() / 7.0f;

            auto keyColour = [&] (int pc, bool white)
            {
                const bool in = (mask >> pc) & 1u;
                if (pc == target) return theme::cyan;
                if (in) return white ? juce::Colour (0xffc9d6ff) : theme::violet;
                return white ? juce::Colour (0xff1c2235) : juce::Colour (0xff0a0c14);
            };

            for (int i = 0; i < 7; ++i)
            {
                auto k = juce::Rectangle<float> (r.getX() + i * ww, r.getY(), ww, r.getHeight()).reduced (1.5f, 0);
                g.setColour (keyColour (whites[i], true));
                g.fillRoundedRectangle (k, 5);
                if (whites[i] == target)
                {
                    g.setColour (theme::cyan.withAlpha (0.4f));
                    g.drawRoundedRectangle (k.expanded (2), 6, 3);
                }
                const bool in = (mask >> whites[i]) & 1u;
                g.setColour (in || whites[i] == target ? juce::Colour (0xff0a0c14) : theme::faint);
                g.setFont (theme::font (11.0f, true));
                g.drawText (fr[whites[i]], k.removeFromBottom (18), juce::Justification::centred);
            }
            for (int i = 0; i < 5; ++i)
            {
                auto k = juce::Rectangle<float> (r.getX() + blackPos[i] * ww - ww * 0.32f, r.getY(), ww * 0.64f, r.getHeight() * 0.6f);
                g.setColour (keyColour (blacks[i], false));
                g.fillRoundedRectangle (k, 4);
                g.setColour (juce::Colour (0x30ffffff));
                g.drawRoundedRectangle (k, 4, 1);
            }

            if (midi)
            {
                g.setColour (theme::pink);
                g.setFont (theme::font (11.0f, true));
                g.drawText ("MIDI", r.removeFromTop (14).removeFromRight (40), juce::Justification::centredRight);
            }
        }

    private:
        uint16_t mask = 0x0AB5;
        int target = -1;
        bool midi = false;
    };

    //==========================================================================
    // Courbe de réponse de l'EQ (calculée sur les mêmes formules que le DSP)
    class EqCurve : public juce::Component
    {
    public:
        void setValues (float hpfHz, float mudDb, float presDb, float airDb, bool enabled)
        {
            const double sr = 48000.0;
            bands[0].highpass (sr, hpfHz);
            bands[1].peak (sr, 300.0, 1.0, mudDb);
            bands[2].peak (sr, 3500.0, 0.8, presDb);
            bands[3].highShelf (sr, 12000.0, airDb);
            on = enabled;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            theme::glassPanel (g, r, 14.0f);
            auto plot = r.reduced (14.0f, 12.0f);

            auto xOf = [&] (double f) { return plot.getX() + (float) (std::log10 (f / 20.0) / std::log10 (1000.0)) * plot.getWidth(); };
            auto yOf = [&] (double db) { return plot.getCentreY() - (float) (db / 18.0) * plot.getHeight() * 0.5f; };

            g.setFont (theme::font (10.0f));
            for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            {
                g.setColour (juce::Colour (0x10ffffff));
                g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
                g.setColour (theme::faint);
                g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                            juce::Rectangle<float> (xOf (f) - 20, plot.getBottom() - 12, 40, 12), juce::Justification::centred);
            }
            for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
            {
                g.setColour (juce::Colour (std::abs (db) < 0.1 ? 0x26ffffff : 0x0cffffff));
                g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
            }

            juce::Path p, fill;
            for (int i = 0; i <= 200; ++i)
            {
                const double f = 20.0 * std::pow (1000.0, i / 200.0);
                double mag = 1.0;
                for (auto& b : bands) mag *= magnitude (b, f, 48000.0);
                const double db = on ? juce::jlimit (-18.0, 18.0, 20.0 * std::log10 (juce::jmax (mag, 1.0e-6))) : 0.0;
                const float x = xOf (f), y = yOf (db);
                if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, yOf (0)); fill.lineTo (x, y); }
                else { p.lineTo (x, y); fill.lineTo (x, y); }
            }
            fill.lineTo (plot.getRight(), yOf (0));
            fill.closeSubPath();

            juce::ColourGradient fg (theme::cyan.withAlpha (0.22f), plot.getX(), plot.getY(),
                                     theme::violet.withAlpha (0.05f), plot.getX(), plot.getBottom(), false);
            g.setGradientFill (fg);
            g.fillPath (fill);
            g.setColour (on ? theme::cyan : theme::faint);
            g.strokePath (p, juce::PathStrokeType (2.2f));
        }

    private:
        static double magnitude (const dsp::Biquad& b, double f, double sr)
        {
            const std::complex<double> z = std::polar (1.0, -2.0 * dsp::kPiD * f / sr);
            const auto num = (double) b.b0 + (double) b.b1 * z + (double) b.b2 * z * z;
            const auto den = 1.0 + (double) b.a1 * z + (double) b.a2 * z * z;
            return std::abs (num / den);
        }

        dsp::Biquad bands[4];
        bool on = true;
    };

    //==========================================================================
    // Carte « bientôt disponible » pour les modules des phases suivantes
    class ComingSoon : public juce::Component
    {
    public:
        ComingSoon (juce::String moduleName, int phase, juce::StringArray features)
            : title (std::move (moduleName)), phaseNum (phase), items (std::move (features)) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            theme::glassPanel (g, r, 18.0f, true);
            auto in = r.reduced (36.0f, 30.0f);

            theme::diamond (g, in.removeFromTop (48).withSizeKeepingCentre (54, 44));
            in.removeFromTop (12);
            g.setColour (theme::text);
            g.setFont (theme::font (30.0f, true));
            g.drawText (title, in.removeFromTop (40), juce::Justification::centred);

            auto badge = in.removeFromTop (30).withSizeKeepingCentre (220, 26);
            g.setColour (theme::violet.withAlpha (0.25f));
            g.fillRoundedRectangle (badge, 13);
            g.setColour (theme::violet);
            g.drawRoundedRectangle (badge, 13, 1);
            g.setFont (theme::font (13.0f, true));
            g.drawText (T ("ARRIVE EN PHASE ") + juce::String (phaseNum), badge, juce::Justification::centred);

            in.removeFromTop (22);
            g.setFont (theme::font (14.5f));
            for (auto& it : items)
            {
                auto line = in.removeFromTop (26);
                g.setColour (theme::cyan);
                g.fillEllipse (line.getX() + 120, line.getCentreY() - 3, 6, 6);
                g.setColour (theme::silver);
                g.drawText (it, line.withTrimmedLeft (136), juce::Justification::centredLeft);
            }
        }

    private:
        juce::String title;
        int phaseNum;
        juce::StringArray items;
    };

    //==========================================================================
    // Carte de section avec titre + interrupteur
    class Section : public juce::Component
    {
    public:
        Section (juce::String t, juce::ToggleButton* power = nullptr) : title (std::move (t)), powerBtn (power)
        {
            if (powerBtn != nullptr) addAndMakeVisible (*powerBtn);
        }

        juce::Rectangle<int> content() const { return getLocalBounds().reduced (14, 10).withTrimmedTop (30); }

        void resized() override
        {
            if (powerBtn != nullptr) powerBtn->setBounds (getWidth() - 50, 10, 40, 22);
        }

        void paint (juce::Graphics& g) override
        {
            theme::glassPanel (g, getLocalBounds().toFloat(), 14.0f);
            g.setColour (theme::text);
            g.setFont (theme::font (14.0f, true));
            g.drawText (title, getLocalBounds().reduced (16, 10).removeFromTop (22), juce::Justification::centredLeft);
        }

    private:
        juce::String title;
        juce::ToggleButton* powerBtn;
    };

    // Répartit les boutons visibles sur une ligne
    inline void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int maxW = 96)
    {
        std::vector<juce::Component*> vis;
        for (auto* c : items) if (c != nullptr && c->isVisible()) vis.push_back (c);
        if (vis.empty()) return;
        const int w = juce::jmin (maxW, area.getWidth() / (int) vis.size());
        const int total = w * (int) vis.size();
        auto row = area.withSizeKeepingCentre (total, juce::jmin (area.getHeight(), w + 40));
        for (auto* c : vis) c->setBounds (row.removeFromLeft (w));
    }
}
