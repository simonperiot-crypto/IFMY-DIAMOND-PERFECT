#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Gestion des presets (thread de l'interface uniquement, jamais dans le thread audio)
//  - presets d'usine (dans le code) + presets utilisateur (fichiers .idpreset)
//  - favoris, undo/redo, comparaison A/B
//  - format texte XML avec valeurs réelles par ID : les anciens presets restent lisibles
//    même quand de nouveaux paramètres arrivent dans les phases suivantes.
class PresetManager
{
public:
    struct Preset
    {
        juce::String name, category;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    struct FactoryPreset
    {
        const char* name;
        const char* category;
        std::vector<std::pair<const char*, float>> values; // valeurs réelles (pas normalisées)
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    static juce::File rootDir();
    static juce::File presetDir();
    static const juce::String extension;
    static const std::vector<FactoryPreset>& factory();

    void refresh();
    const juce::Array<Preset>& all() const { return list; }
    juce::StringArray categories() const;

    void load (int index);
    void loadFactory (int factoryIndex);
    void next (int direction);
    int currentIndex() const { return current; }
    juce::String currentName() const { return currentPresetName; }
    juce::String currentCategory() const { return currentPresetCategory; }
    void setCurrentName (const juce::String& name, const juce::String& category);
    int currentFactoryIndex() const;

    bool save (const juce::String& name, const juce::String& category);
    bool rename (int index, const juce::String& newName);
    bool remove (int index);
    bool importFile (const juce::File& f);
    bool exportCurrent (const juce::File& destination) const;
    bool exists (const juce::String& name) const;

    bool isFavourite (const juce::String& name) const { return favourites.contains (name); }
    void toggleFavourite (const juce::String& name);

    bool keepKey = true; // charger un preset garde la tonalité/gamme choisie

    // Historique (undo/redo) et A/B
    void poll();               // à appeler régulièrement (timer de l'interface)
    bool canUndo() const { return historyPos > 0; }
    bool canRedo() const { return historyPos + 1 < (int) history.size(); }
    void undo();
    void redo();
    void switchAB();
    void copyAtoB();
    int  abSlot() const { return ab; }

private:
    using Snapshot = std::vector<float>;
    Snapshot capture() const;
    void restore (const Snapshot& s);
    void applyValues (const std::vector<std::pair<juce::String, float>>& values);
    void loadFavourites();
    void saveFavourites() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Preset> list;
    juce::StringArray favourites;
    int current = 0;
    juce::String currentPresetName { "IFMY Signature" }, currentPresetCategory { "Signature" };

    std::vector<Snapshot> history;
    int historyPos = -1;
    Snapshot pending;
    int stableTicks = 0;
    bool restoring = false;

    Snapshot slots[2];
    int ab = 0;
};
