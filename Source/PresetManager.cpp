#include "PresetManager.h"
#include "Params.h"

const juce::String PresetManager::extension { ".idpreset" };

// ============================================================================
//  Presets d'usine — Phase 1 (les 50 presets complets arrivent en Phase 3,
//  quand HARMONY / VOCODER / BENDER seront là pour les rendre vraiment uniques)
// ============================================================================
const std::vector<PresetManager::FactoryPreset>& PresetManager::factory()
{
    static const std::vector<FactoryPreset> l =
    {
        { "IFMY Signature", "Signature",
          { { "tune_retune", 30 }, { "tune_amount", 90 }, { "tune_humanize", 35 }, { "eq_hpf", 90 }, { "eq_mud", -2 },
            { "eq_pres", 2.5f }, { "eq_air", 3 }, { "cmp_thr", -20 }, { "cmp_ratio", 3.5f }, { "cmp_gain", 4 },
            { "dbl_on", 1 }, { "dbl_mix", 18 }, { "dly_on", 1 }, { "dly_div", 5 }, { "dly_mix", 10 }, { "rev_mix", 14 } } },

        { "Diamond Clean", "Autotune naturel",
          { { "tune_retune", 70 }, { "tune_amount", 75 }, { "tune_humanize", 55 }, { "eq_hpf", 85 }, { "eq_air", 2.5f },
            { "cmp_thr", -18 }, { "cmp_ratio", 2.5f }, { "cmp_gain", 2.5f }, { "rev_mix", 9 } } },

        { "Naturel Live", "Autotune naturel",
          { { "tune_retune", 120 }, { "tune_amount", 55 }, { "tune_humanize", 70 }, { "eq_hpf", 70 }, { "cmp_thr", -16 },
            { "cmp_ratio", 2.5f }, { "cmp_gain", 2 }, { "rev_mix", 10 } } },

        { "Hard Tune Ice", "Hard tune",
          { { "tune_retune", 0 }, { "tune_amount", 100 }, { "eq_hpf", 110 }, { "eq_pres", 3 }, { "eq_air", 4 },
            { "cmp_thr", -24 }, { "cmp_ratio", 5 }, { "cmp_gain", 6 }, { "sat_on", 1 }, { "sat_drive", 7 }, { "sat_mix", 30 },
            { "dly_on", 1 }, { "dly_div", 5 }, { "dly_mix", 14 }, { "rev_mix", 15 } } },

        { "Trap Melo", "Trap / Drill",
          { { "tune_retune", 4 }, { "tune_amount", 100 }, { "tune_humanize", 8 }, { "eq_hpf", 120 }, { "eq_mud", -3 },
            { "eq_pres", 3.5f }, { "eq_air", 5 }, { "cmp_thr", -26 }, { "cmp_ratio", 6 }, { "cmp_gain", 7 },
            { "dbl_on", 1 }, { "dbl_mix", 22 }, { "dly_on", 1 }, { "dly_div", 3 }, { "dly_fb", 38 }, { "dly_mix", 16 },
            { "rev_size", 65 }, { "rev_mix", 16 } } },

        { "Drill UK Dark", "Trap / Drill",
          { { "tune_retune", 10 }, { "tune_amount", 95 }, { "eq_hpf", 140 }, { "eq_mud", -4 }, { "eq_pres", 2 }, { "eq_air", 1 },
            { "cmp_thr", -28 }, { "cmp_ratio", 8 }, { "cmp_att", 2 }, { "cmp_gain", 8 }, { "sat_on", 1 }, { "sat_drive", 10 },
            { "sat_mix", 35 }, { "dly_on", 1 }, { "dly_div", 6 }, { "dly_mix", 10 }, { "rev_size", 35 }, { "rev_mix", 8 } } },

        { "Afro Sunset", "Afro / Pop",
          { { "tune_retune", 8 }, { "tune_amount", 95 }, { "tune_humanize", 10 }, { "eq_hpf", 100 }, { "eq_air", 3 },
            { "cmp_thr", -22 }, { "cmp_ratio", 4 }, { "dly_on", 1 }, { "dly_div", 3 }, { "dly_fb", 35 }, { "dly_mix", 18 },
            { "rev_mix", 18 } } },

        { "Pop Brillante", "Afro / Pop",
          { { "tune_retune", 35 }, { "tune_amount", 85 }, { "tune_humanize", 30 }, { "eq_hpf", 100 }, { "eq_pres", 3 },
            { "eq_air", 5 }, { "cmp_thr", -22 }, { "cmp_ratio", 4 }, { "cmp_gain", 5 }, { "dbl_on", 1 }, { "dbl_mix", 20 },
            { "rev_mix", 15 } } },

        { "R&B Soie", "R&B / Soul",
          { { "tune_retune", 22 }, { "tune_amount", 90 }, { "tune_humanize", 50 }, { "eq_hpf", 80 }, { "eq_mud", -3 },
            { "eq_air", 4 }, { "cmp_thr", -20 }, { "cmp_ratio", 3 }, { "dbl_on", 1 }, { "dbl_mix", 28 }, { "dly_on", 1 },
            { "dly_div", 2 }, { "dly_mix", 10 }, { "rev_size", 72 }, { "rev_mix", 22 } } },

        { "Soul Velours", "R&B / Soul",
          { { "tune_retune", 90 }, { "tune_amount", 60 }, { "tune_humanize", 65 }, { "eq_hpf", 70 }, { "eq_mud", -1 },
            { "eq_pres", 1 }, { "eq_air", 2 }, { "cmp_thr", -18 }, { "cmp_ratio", 3 }, { "cmp_att", 12 }, { "cmp_gain", 3 },
            { "sat_on", 1 }, { "sat_drive", 5 }, { "sat_mix", 35 }, { "rev_size", 60 }, { "rev_damp", 60 }, { "rev_mix", 18 } } },

        { "Robot Chrome", "Voix creatives",
          { { "tune_scale", 4 }, { "tune_retune", 0 }, { "tune_amount", 100 }, { "eq_hpf", 140 }, { "sat_on", 1 },
            { "sat_drive", 10 }, { "sat_mix", 45 }, { "dbl_on", 1 }, { "dbl_mix", 25 }, { "rev_mix", 10 } } },

        { "Lo-Fi Bedroom", "Lo-fi / Ambiance",
          { { "tune_retune", 45 }, { "tune_amount", 80 }, { "eq_hpf", 260 }, { "eq_air", -4 }, { "sat_on", 1 },
            { "sat_drive", 15 }, { "sat_mix", 60 }, { "dly_on", 1 }, { "dly_div", 2 }, { "dly_tone", 2500 }, { "dly_mix", 16 },
            { "rev_size", 40 }, { "rev_damp", 80 }, { "rev_mix", 24 } } },

        { "Cathedrale", "Cinema / Epique",
          { { "tune_retune", 40 }, { "tune_amount", 85 }, { "tune_humanize", 40 }, { "eq_air", 3 }, { "dly_on", 1 },
            { "dly_div", 2 }, { "dly_fb", 40 }, { "dly_mix", 14 }, { "rev_size", 95 }, { "rev_damp", 25 }, { "rev_mix", 34 } } },

        // ---- Phase 2 : Classic, Flex-Tune, vibrato, gorge / formants ----
        { "Classic 98", "Hard tune",
          { { "tune_mode", 1 }, { "tune_retune", 0 }, { "tune_amount", 100 }, { "eq_hpf", 120 }, { "eq_pres", 3 },
            { "eq_air", 2 }, { "cmp_thr", -22 }, { "cmp_ratio", 4 }, { "cmp_gain", 4 }, { "dly_on", 1 }, { "dly_div", 5 },
            { "dly_mix", 8 }, { "rev_mix", 10 } } },

        { "Flex Naturel", "Autotune naturel",
          { { "tune_retune", 15 }, { "tune_amount", 100 }, { "tune_humanize", 30 }, { "tune_flex", 70 }, { "eq_hpf", 80 },
            { "eq_air", 2.5f }, { "cmp_thr", -18 }, { "cmp_ratio", 3 }, { "cmp_gain", 3 }, { "rev_mix", 11 } } },

        { "Diva Vibrato", "R&B / Soul",
          { { "tune_retune", 20 }, { "tune_amount", 95 }, { "tune_humanize", 40 }, { "vib_depth", 35 }, { "vib_rate", 5.2f },
            { "vib_delay", 300 }, { "eq_hpf", 85 }, { "eq_mud", -2 }, { "eq_air", 4 }, { "cmp_thr", -20 }, { "cmp_ratio", 3 },
            { "dbl_on", 1 }, { "dbl_mix", 18 }, { "rev_size", 70 }, { "rev_mix", 20 } } },

        { "Gorge Profonde", "Voix creatives",
          { { "tune_retune", 10 }, { "tune_transpose", -5 }, { "tune_throat", 128 }, { "tune_formant", 1 }, { "eq_hpf", 60 },
            { "eq_mud", -3 }, { "cmp_thr", -22 }, { "cmp_ratio", 4 }, { "sat_on", 1 }, { "sat_drive", 8 }, { "sat_mix", 30 },
            { "rev_size", 60 }, { "rev_mix", 14 } } },

        { "Chipmunk Pop", "Voix creatives",
          { { "tune_retune", 0 }, { "tune_transpose", 7 }, { "tune_formant", 0 }, { "tune_throat", 88 }, { "eq_hpf", 160 },
            { "eq_air", 3 }, { "cmp_thr", -20 }, { "cmp_ratio", 4 }, { "dly_on", 1 }, { "dly_div", 7 }, { "dly_mix", 12 },
            { "rev_mix", 10 } } },

        { "Voix seule (sans effets)", "Utilitaire",
          { { "eq_on", 0 }, { "dyn_on", 0 }, { "fx_on", 0 } } },
    };
    return l;
}

// ============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    loadFavourites();
    refresh();
}

juce::File PresetManager::rootDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support").getChildFile ("IFMY Diamond Perfect");
}

juce::File PresetManager::presetDir()
{
    auto d = rootDir().getChildFile ("Presets");
    if (! d.isDirectory()) d.createDirectory();
    return d;
}

void PresetManager::refresh()
{
    list.clearQuick();
    const auto& f = factory();
    for (int i = 0; i < (int) f.size(); ++i)
        list.add ({ f[(size_t) i].name, f[(size_t) i].category, true, i, {} });

    juce::Array<juce::File> files;
    presetDir().findChildFiles (files, juce::File::findFiles, true, "*" + extension);
    files.sort();
    for (auto& file : files)
        if (auto xml = juce::XmlDocument::parse (file))
            if (xml->hasTagName ("IFMYPreset"))
                list.add ({ xml->getStringAttribute ("name", file.getFileNameWithoutExtension()),
                            xml->getStringAttribute ("category", "Mes presets"), false, -1, file });

    // Retrouver le preset courant après un rafraîchissement
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == currentPresetName) { current = i; return; }
}

juce::StringArray PresetManager::categories() const
{
    juce::StringArray c;
    for (auto& p : list) c.addIfNotAlreadyThere (p.category);
    return c;
}

void PresetManager::setCurrentName (const juce::String& name, const juce::String& category)
{
    currentPresetName = name;
    currentPresetCategory = category;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == name) { current = i; break; }
}

int PresetManager::currentFactoryIndex() const
{
    return juce::isPositiveAndBelow (current, list.size()) ? list[current].factoryIndex : -1;
}

// ----------------------------------------------------------------------------
void PresetManager::applyValues (const std::vector<std::pair<juce::String, float>>& values)
{
    restoring = true;
    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (pid::isSessionParam (rp->paramID) || (keepKey && pid::isKeyParam (rp->paramID))) continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    for (auto& [id, value] : values)
    {
        if (pid::isSessionParam (id) || (keepKey && pid::isKeyParam (id))) continue;
        if (auto* rp = apvts.getParameter (id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
            rp->endChangeGesture();
        }
    }
    restoring = false;
}

void PresetManager::loadFactory (int factoryIndex)
{
    const auto& f = factory();
    if (! juce::isPositiveAndBelow (factoryIndex, (int) f.size())) return;
    std::vector<std::pair<juce::String, float>> v;
    for (auto& [id, val] : f[(size_t) factoryIndex].values) v.push_back ({ id, val });
    applyValues (v);
    setCurrentName (f[(size_t) factoryIndex].name, f[(size_t) factoryIndex].category);
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size())) return;
    const auto p = list[index];
    if (p.factory) { loadFactory (p.factoryIndex); return; }

    if (auto xml = juce::XmlDocument::parse (p.file))
    {
        std::vector<std::pair<juce::String, float>> v;
        for (auto* e : xml->getChildWithTagNameIterator ("Param"))
            v.push_back ({ e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value") });
        applyValues (v);
        setCurrentName (p.name, p.category);
    }
}

void PresetManager::next (int direction)
{
    if (list.isEmpty()) return;
    load ((current + direction + list.size()) % list.size());
}

bool PresetManager::exists (const juce::String& name) const
{
    for (auto& p : list) if (p.name.equalsIgnoreCase (name)) return true;
    return false;
}

static juce::String safeFileName (const juce::String& n)
{
    return juce::File::createLegalFileName (n.trim()).substring (0, 80);
}

bool PresetManager::save (const juce::String& name, const juce::String& category)
{
    const auto n = name.trim();
    if (n.isEmpty()) return false;

    // On n'écrase jamais un preset d'usine
    for (auto& p : list) if (p.factory && p.name.equalsIgnoreCase (n)) return false;

    juce::XmlElement xml ("IFMYPreset");
    xml.setAttribute ("name", n);
    xml.setAttribute ("category", category.trim().isEmpty() ? "Mes presets" : category.trim());
    xml.setAttribute ("version", 1);
    xml.setAttribute ("plugin", "IFMY Diamond Perfect");

    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (pid::isSessionParam (rp->paramID)) continue;
            auto* e = xml.createNewChildElement ("Param");
            e->setAttribute ("id", rp->paramID);
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }

    const auto file = presetDir().getChildFile (safeFileName (n) + extension);
    if (! xml.writeTo (file)) return false;
    currentPresetName = n;
    currentPresetCategory = xml.getStringAttribute ("category");
    refresh();
    return true;
}

bool PresetManager::rename (int index, const juce::String& newName)
{
    if (! juce::isPositiveAndBelow (index, list.size()) || list[index].factory) return false;
    const auto n = newName.trim();
    if (n.isEmpty() || exists (n)) return false;
    auto xml = juce::XmlDocument::parse (list[index].file);
    if (xml == nullptr) return false;
    xml->setAttribute ("name", n);
    const auto dest = presetDir().getChildFile (safeFileName (n) + extension);
    if (! xml->writeTo (dest)) return false;
    if (dest != list[index].file) list[index].file.deleteFile();
    if (favourites.contains (list[index].name)) { favourites.removeString (list[index].name); favourites.add (n); saveFavourites(); }
    if (currentPresetName == list[index].name) currentPresetName = n;
    refresh();
    return true;
}

bool PresetManager::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, list.size()) || list[index].factory) return false;
    const bool ok = list[index].file.moveToTrash() || list[index].file.deleteFile();
    refresh();
    return ok;
}

bool PresetManager::importFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("IFMYPreset")) return false;
    auto name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    if (exists (name)) { name << " (import)"; xml->setAttribute ("name", name); }
    const bool ok = xml->writeTo (presetDir().getChildFile (safeFileName (name) + extension));
    refresh();
    return ok;
}

bool PresetManager::exportCurrent (const juce::File& destination) const
{
    juce::XmlElement xml ("IFMYPreset");
    xml.setAttribute ("name", currentPresetName);
    xml.setAttribute ("category", currentPresetCategory);
    xml.setAttribute ("version", 1);
    xml.setAttribute ("plugin", "IFMY Diamond Perfect");
    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (pid::isSessionParam (rp->paramID)) continue;
            auto* e = xml.createNewChildElement ("Param");
            e->setAttribute ("id", rp->paramID);
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    return xml.writeTo (destination.withFileExtension (extension));
}

// ----------------------------------------------------------------------------
void PresetManager::loadFavourites()
{
    if (auto xml = juce::XmlDocument::parse (rootDir().getChildFile ("favoris.xml")))
        for (auto* e : xml->getChildWithTagNameIterator ("Fav"))
            favourites.addIfNotAlreadyThere (e->getStringAttribute ("name"));
}

void PresetManager::saveFavourites() const
{
    juce::XmlElement xml ("Favoris");
    for (auto& f : favourites) xml.createNewChildElement ("Fav")->setAttribute ("name", f);
    rootDir().createDirectory();
    xml.writeTo (rootDir().getChildFile ("favoris.xml"));
}

void PresetManager::toggleFavourite (const juce::String& name)
{
    if (favourites.contains (name)) favourites.removeString (name);
    else favourites.add (name);
    saveFavourites();
}

// ----------------------------------------------------------------------------
PresetManager::Snapshot PresetManager::capture() const
{
    Snapshot s;
    for (auto* param : apvts.processor.getParameters())
        s.push_back (param->getValue());
    return s;
}

void PresetManager::restore (const Snapshot& s)
{
    restoring = true;
    const auto& params = apvts.processor.getParameters();
    for (int i = 0; i < params.size() && i < (int) s.size(); ++i)
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
        if (rp == nullptr || rp->paramID == pid::bypass) continue;
        if (std::abs (rp->getValue() - s[(size_t) i]) < 1.0e-6f) continue;
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (s[(size_t) i]);
        rp->endChangeGesture();
    }
    restoring = false;
    pending = capture();
}

void PresetManager::poll()
{
    // Un état est ajouté à l'historique quand les réglages restent stables ~0,4 s
    auto now = capture();
    if (history.empty()) { history.push_back (now); historyPos = 0; pending = now; return; }
    if (now != pending) { pending = now; stableTicks = 0; return; }
    if (++stableTicks < 4 || now == history[(size_t) historyPos]) return;

    history.resize ((size_t) historyPos + 1);
    history.push_back (now);
    if (history.size() > 100) history.erase (history.begin());
    historyPos = (int) history.size() - 1;
}

void PresetManager::undo()
{
    if (! canUndo()) return;
    --historyPos;
    restore (history[(size_t) historyPos]);
}

void PresetManager::redo()
{
    if (! canRedo()) return;
    ++historyPos;
    restore (history[(size_t) historyPos]);
}

void PresetManager::switchAB()
{
    slots[ab] = capture();
    ab = 1 - ab;
    if (slots[ab].empty()) slots[ab] = slots[1 - ab];
    restore (slots[ab]);
}

void PresetManager::copyAtoB()
{
    slots[ab] = capture();
    slots[1 - ab] = slots[ab];
}
