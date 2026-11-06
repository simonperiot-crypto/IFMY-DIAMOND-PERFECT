#pragma once
#include "PluginProcessor.h"
#include "ui/Pages.h"

class DiamondEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit DiamondEditor (DiamondProcessor&);
    ~DiamondEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int kW = 1180, kH = 720;

private:
    //==========================================================================
    // Bouton de module dans le rack (clic = ouvrir, LED = on/off)
    class ModuleButton : public juce::Component
    {
    public:
        ModuleButton (juce::String n, juce::String sub, juce::RangedAudioParameter* power, int phase)
            : name (std::move (n)), subtitle (std::move (sub)), param (power), comingPhase (phase) {}

        std::function<void()> onSelect;
        bool selected = false;

        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
        bool isOn() const { return param == nullptr || param->getValue() > 0.5f; }

        juce::String name, subtitle;
        juce::RangedAudioParameter* param;
        int comingPhase;
        bool hover = false, lastOn = true;

    private:
        juce::Rectangle<float> ledArea() const { return { (float) getWidth() - 34.0f, (float) getHeight() * 0.5f - 9.0f, 18.0f, 18.0f }; }
    };

    //==========================================================================
    class Content : public juce::Component
    {
    public:
        explicit Content (DiamondEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        DiamondEditor& ed;
    };

    void timerCallback() override;
    void layoutContent();
    void selectModule (int index);
    void setExpert (bool e);
    void showPresetMenu();
    void showMoreMenu();
    void savePresetDialog();
    void renameDialog();
    void refreshPresetLabel();

    DiamondProcessor& proc;
    theme::LookAndFeel lnf;
    Content content { *this };
    juce::TooltipWindow tooltips { this, 600 };

    // En-tête
    juce::TextButton prevBtn, presetBtn, nextBtn, favBtn, saveBtn, moreBtn, aBtn { "A" }, bBtn { "B" },
                     undoBtn, redoBtn, expertBtn { "EXPERT" }, bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    // Rack + pages
    juce::OwnedArray<ModuleButton> modules;
    juce::OwnedArray<juce::Component> pages;
    int currentModule = 0;

    // Vumètres
    ui::Meter inMeter { "IN" }, outMeter { "OUT" }, grMeter { "COMP", true }, dsMeter { "DESS", true };

    std::unique_ptr<juce::FileChooser> chooser;
    int tickCount = 0;
    float cpuShown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiamondEditor)
};
