#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// IFMY DIAMOND PERFECT — identité visuelle : verre sombre, reflets diamant, néons cyan / violet / argent
namespace theme
{
    inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    inline const juce::Colour bg0     { 0xff05060b };
    inline const juce::Colour bg1     { 0xff0c0f1a };
    inline const juce::Colour bg2     { 0xff141a2b };
    inline const juce::Colour glass   { 0x0fffffff };
    inline const juce::Colour glassHi { 0x1fffffff };
    inline const juce::Colour edge    { 0x26ffffff };
    inline const juce::Colour cyan    { 0xff3ef2ff };
    inline const juce::Colour violet  { 0xffa66bff };
    inline const juce::Colour pink    { 0xffff5fd2 };
    inline const juce::Colour silver  { 0xffd9e2ec };
    inline const juce::Colour text    { 0xffeef3ff };
    inline const juce::Colour dim     { 0xff8a93ab };
    inline const juce::Colour faint   { 0xff4a5168 };
    inline const juce::Colour warn    { 0xffffb84d };
    inline const juce::Colour hot     { 0xffff4d6d };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::ColourGradient neon (juce::Rectangle<float> r)
    {
        juce::ColourGradient g (cyan, r.getX(), r.getY(), violet, r.getRight(), r.getBottom(), false);
        g.addColour (0.55, juce::Colour (0xff7d9bff));
        return g;
    }

    // Panneau en verre : fond translucide, liseré lumineux, reflet en haut
    inline void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f, bool highlighted = false)
    {
        juce::ColourGradient fill (juce::Colour (0x1affffff), r.getX(), r.getY(),
                                   juce::Colour (0x08ffffff), r.getX(), r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, radius);

        auto shine = r.withHeight (juce::jmin (36.0f, r.getHeight() * 0.35f)).reduced (1.0f);
        juce::ColourGradient sh (juce::Colour (0x14ffffff), shine.getX(), shine.getY(),
                                 juce::Colours::transparentWhite, shine.getX(), shine.getBottom(), false);
        g.setGradientFill (sh);
        g.fillRoundedRectangle (shine, radius - 1.0f);

        if (highlighted)
        {
            g.setGradientFill (neon (r));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.4f);
        }
        else
        {
            g.setColour (edge);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        }
    }

    // Petit diamant (logo)
    inline void diamond (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path p;
        const float cx = r.getCentreX(), w = r.getWidth(), h = r.getHeight(), x = r.getX(), y = r.getY();
        p.startNewSubPath (x + w * 0.22f, y);
        p.lineTo (x + w * 0.78f, y);
        p.lineTo (x + w, y + h * 0.32f);
        p.lineTo (cx, y + h);
        p.lineTo (x, y + h * 0.32f);
        p.closeSubPath();
        g.setGradientFill (neon (r));
        g.fillPath (p);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        juce::Path facets;
        facets.startNewSubPath (x, y + h * 0.32f); facets.lineTo (x + w, y + h * 0.32f);
        facets.startNewSubPath (x + w * 0.22f, y); facets.lineTo (x + w * 0.38f, y + h * 0.32f); facets.lineTo (cx, y + h);
        facets.startNewSubPath (x + w * 0.78f, y); facets.lineTo (x + w * 0.62f, y + h * 0.32f); facets.lineTo (cx, y + h);
        facets.startNewSubPath (x + w * 0.38f, y + h * 0.32f); facets.lineTo (cx, y); facets.lineTo (x + w * 0.62f, y + h * 0.32f);
        g.strokePath (facets, juce::PathStrokeType (0.8f));
    }

    //==========================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x14ffffff));
            setColour (juce::ComboBox::outlineColourId, edge);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, cyan);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf20c0f1a));
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x553ef2ff));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::PopupMenu::headerTextColourId, cyan);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0x14ffffff));
            setColour (juce::TextButton::buttonOnColourId, juce::Colour (0x553ef2ff));
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            setColour (juce::Label::textColourId, text);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0c0f1a));
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::TextEditor::outlineColourId, edge);
            setColour (juce::TextEditor::focusedOutlineColourId, cyan);
            setColour (juce::AlertWindow::backgroundColourId, bg1);
            setColour (juce::AlertWindow::textColourId, text);
            setColour (juce::AlertWindow::outlineColourId, cyan);
            setColour (juce::Slider::textBoxTextColourId, text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x553ef2ff));
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider& s) override
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
            const float size = juce::jmin (r.getWidth(), r.getHeight());
            r = r.withSizeKeepingCentre (size, size);
            const auto c = r.getCentre();
            const float rad = size * 0.5f;
            const float ang = a0 + pos * (a1 - a0);
            const bool on = s.isEnabled();

            // halo
            if (on)
            {
                juce::ColourGradient halo (cyan.withAlpha (0.10f + 0.12f * pos), c.x, c.y,
                                           juce::Colours::transparentBlack, c.x + rad * 1.25f, c.y, true);
                g.setGradientFill (halo);
                g.fillEllipse (r.expanded (rad * 0.25f));
            }

            // piste
            juce::Path track;
            track.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, a0, a1, true);
            g.setColour (juce::Colour (0x22ffffff));
            g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // valeur (depuis le centre pour les paramètres bipolaires)
            const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
            const float from = bipolar ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
            juce::Path val;
            val.addCentredArc (c.x, c.y, rad - 3, rad - 3, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setGradientFill (neon (r));
            if (! on) g.setColour (faint);
            g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // bouton en verre
            auto knob = r.reduced (rad * 0.28f);
            juce::ColourGradient body (juce::Colour (0xff2a3350), knob.getX(), knob.getY(),
                                       juce::Colour (0xff0b0e18), knob.getRight(), knob.getBottom(), false);
            g.setGradientFill (body);
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0x40ffffff));
            g.drawEllipse (knob, 1.0f);

            // index
            const float ir = knob.getWidth() * 0.5f;
            juce::Point<float> p1 (c.x + std::sin (ang) * ir * 0.35f, c.y - std::cos (ang) * ir * 0.35f);
            juce::Point<float> p2 (c.x + std::sin (ang) * ir * 0.85f, c.y - std::cos (ang) * ir * 0.85f);
            g.setColour (on ? juce::Colours::white : dim);
            g.drawLine ({ p1, p2 }, 2.2f);
        }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (juce::Colour (0x12ffffff));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (box.hasKeyboardFocus (true) ? cyan : edge);
            g.drawRoundedRectangle (r, 8.0f, 1.0f);

            juce::Path arrow;
            const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
            arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
            g.setColour (cyan);
            g.fillPath (arrow);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f, true); }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        juce::Font getPopupMenuFont() override { return font (14.0f); }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState();
            g.setColour (juce::Colour (on ? 0x2a3ef2ff : (down ? 0x2affffff : (over ? 0x1cffffff : 0x10ffffff))));
            g.fillRoundedRectangle (r, 8.0f);
            if (on) { g.setGradientFill (neon (r)); g.drawRoundedRectangle (r, 8.0f, 1.3f); }
            else    { g.setColour (over ? glassHi.withAlpha (0.4f) : edge); g.drawRoundedRectangle (r, 8.0f, 1.0f); }
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (14.0f, h * 0.5f), true); }

        // Interrupteur LED (on/off des modules)
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat();
            const bool on = b.getToggleState();
            auto pill = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
            g.setColour (on ? cyan.withAlpha (0.25f) : juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (pill, 8.0f);
            g.setColour (on ? cyan : (over ? dim : faint));
            g.drawRoundedRectangle (pill, 8.0f, 1.0f);
            auto dot = juce::Rectangle<float> (12, 12).withCentre ({ on ? pill.getRight() - 8.0f : pill.getX() + 8.0f, pill.getCentreY() });
            if (on)
            {
                g.setColour (cyan.withAlpha (0.35f));
                g.fillEllipse (dot.expanded (4));
            }
            g.setColour (on ? juce::Colours::white : dim);
            g.fillEllipse (dot);

            g.setColour (on ? text : dim);
            g.setFont (font (13.0f, true));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xf20a0d17));
            g.setColour (cyan.withAlpha (0.4f));
            g.drawRect (0, 0, w, h);
        }

        void drawAlertBox (juce::Graphics& g, juce::AlertWindow& a, const juce::Rectangle<int>& textArea, juce::TextLayout& tl) override
        {
            auto r = a.getLocalBounds().toFloat();
            g.fillAll (bg1);
            g.setGradientFill (neon (r));
            g.drawRect (r, 1.5f);
            tl.draw (g, textArea.toFloat());
        }
    };
}
