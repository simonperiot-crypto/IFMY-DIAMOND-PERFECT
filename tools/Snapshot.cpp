// Outil de dev : génère des captures PNG de l'interface (sans DAW)
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "../Source/PluginProcessor.h"
#include "../Source/ui/Pages.h"


int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? argv[1] : "/tmp");
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    proc->setPlayConfigDetails (2, 2, 48000, 512);
    proc->prepareToPlay (48000, 512);

    // Signal de test : voix synthétique qui glisse (pour animer la courbe de justesse)
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double ph = 0;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditorIfNeeded());
    auto* timer = dynamic_cast<juce::Timer*> (ed.get());

    const char* modules[] = { "tune", "graph", "bender", "harmony", "vocoder", "dynamics", "eq", "fx", "master" };
    for (int m : { 0, 5, 6, 7, 8, 3 })
    {
        ed->getProperties().set ("m", m);
        // sélection du module via un clic simulé sur le rack
        auto* content = ed->getChildComponent (0);
        auto* button = content->getChildComponent (12 + m);
        if (button != nullptr)
            button->mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 20.0f, 20.0f },
                juce::ModifierKeys(), 0, 0, 0, 0, 0, button, button, juce::Time::getCurrentTime(), { 20.0f, 20.0f }, juce::Time::getCurrentTime(), 1, false));

        for (int frame = 0; frame < 260; ++frame)
        {
            for (int b = 0; b < 3; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const double t = (frame * 3 + b) * 512 + i;
                    const double hz = 196.0 * std::pow (2.0, (3.0 * std::sin (t / 48000.0 * 1.3) + 0.35 * std::sin (t / 48000.0 * 33.0)) / 12.0);
                    ph += 2 * juce::MathConstants<double>::pi * hz / 48000.0;
                    const float s = 0.3f * (float) (std::sin (ph) + 0.4 * std::sin (2 * ph) + 0.2 * std::sin (3 * ph));
                    buf.setSample (0, i, s); buf.setSample (1, i, s);
                }
                proc->processBlock (buf, midi);
            }
            if (timer != nullptr) timer->timerCallback();
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (juce::String ("ui_") + modules[m] + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    }
    ed.reset();

    auto save = [&] (juce::Component& c, const char* name)
    {
        auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    };

    // Mode expert
    auto* dp = dynamic_cast<DiamondProcessor*> (proc.get());
    dp->uiState.setProperty ("expert", true, nullptr);
    dp->uiState.setProperty ("module", 0, nullptr);
    ed.reset (proc->createEditorIfNeeded());
    if (auto* t = dynamic_cast<juce::Timer*> (ed.get()))
        for (int frame = 0; frame < 200; ++frame)
        {
            for (int b = 0; b < 3; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const double tt = (frame * 3 + b) * 512 + i;
                    const double hz = 220.0 * std::pow (2.0, (2.0 * std::sin (tt / 48000.0 * 1.1)) / 12.0);
                    ph += 2 * juce::MathConstants<double>::pi * hz / 48000.0;
                    const float s = 0.3f * (float) (std::sin (ph) + 0.5 * std::sin (2 * ph));
                    buf.setSample (0, i, s); buf.setSample (1, i, s);
                }
                proc->processBlock (buf, midi);
            }
            t->timerCallback();
        }
    save (*ed, "ui_tune_expert.png");
    ed.reset();

    // Key Finder : suite d'accords Sol majeur
    {
        ui::KeyFinderPanel<DiamondProcessor> panel (*dp);
        theme::LookAndFeel lnf;
        panel.setLookAndFeel (&lnf);
        for (auto* c : panel.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                if (b->getButtonText().startsWith ("STOP") || b->getButtonText().contains (juce::String::fromUTF8 ("ÉCOUTER")))
                    b->onClick();
        const int chords[4][4] = { { 43, 55, 59, 62 }, { 48, 60, 64, 67 }, { 50, 62, 66, 69 }, { 52, 64, 67, 71 } };
        double t = 0;
        for (int sec = 0; sec < 16; ++sec)
        {
            for (int blk = 0; blk < 94; ++blk)
            {
                for (int i = 0; i < 512; ++i, t += 1.0 / 48000.0)
                {
                    double s = 0;
                    for (int n : chords[sec % 4])
                    {
                        const double f = 440.0 * std::pow (2.0, (n - 69) / 12.0);
                        s += std::sin (2 * juce::MathConstants<double>::pi * f * t) + 0.4 * std::sin (4 * juce::MathConstants<double>::pi * f * t);
                    }
                    buf.setSample (0, i, (float) (0.06 * s)); buf.setSample (1, i, (float) (0.06 * s));
                }
                proc->processBlock (buf, midi);
            }
            dynamic_cast<juce::Timer&> (panel).timerCallback();
        }
        save (panel, "ui_keyfinder.png");
        panel.setLookAndFeel (nullptr);
    }
    return 0;
}
