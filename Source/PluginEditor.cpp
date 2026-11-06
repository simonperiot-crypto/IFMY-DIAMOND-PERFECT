#include "PluginEditor.h"

using theme::T;

namespace
{
    struct ModuleDef { const char* name; const char* sub; const char* power; int phase; };

    // Ordre du rack (fixe en Phase 1 ; le glisser-déposer arrive avec les modules avancés)
    const ModuleDef kModules[] =
    {
        { "TUNE",     "Autotune temps réel",   pid::tuneOn, 0 },
        { "GRAPH",    "Édition note par note", nullptr,     6 },
        { "BENDER",   "Pitch & formant",       nullptr,     5 },
        { "HARMONY",  "15 styles d'harmonie",  nullptr,     4 },
        { "VOCODER",  "Vocodeur & synth voix", nullptr,     5 },
        { "DYNAMICS", "Gate · De-ess · Comp",  pid::dynOn,  0 },
        { "EQ",       "EQ vocal + Air",        pid::eqOn,   0 },
        { "FX",       "Doubleur · Delay · Rev", pid::fxOn,  0 },
        { "MASTER",   "Gains & sécurité",      nullptr,     0 },
    };
}

//==============================================================================
void DiamondEditor::ModuleButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool soon = comingPhase > 0;

    if (selected)
    {
        juce::ColourGradient sg (theme::cyan.withAlpha (0.16f), r.getX(), r.getY(), theme::violet.withAlpha (0.10f), r.getRight(), r.getY(), false);
        g.setGradientFill (sg);
        g.fillRoundedRectangle (r, 12.0f);
        g.setGradientFill (theme::neon (r));
        g.drawRoundedRectangle (r, 12.0f, 1.4f);
        g.fillRoundedRectangle (r.getX() + 1, r.getY() + 12, 3, r.getHeight() - 24, 1.5f);
    }
    else
    {
        g.setColour (juce::Colour (hover ? 0x16ffffff : 0x0bffffff));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (r, 12.0f, 1.0f);
    }

    auto t = r.reduced (16, 8).withTrimmedRight (30);
    g.setColour (soon ? theme::dim : theme::text);
    g.setFont (theme::font (15.0f, true));
    g.drawText (name, t.removeFromTop (t.getHeight() * 0.55f), juce::Justification::bottomLeft);
    g.setColour (soon ? theme::faint : theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText (soon ? T ("Phase ") + juce::String (comingPhase) : subtitle, t, juce::Justification::topLeft);

    auto led = ledArea();
    if (soon)
    {
        g.setColour (theme::faint);
        g.drawEllipse (led.reduced (4), 1.0f);
    }
    else if (param != nullptr)
    {
        const bool on = isOn();
        if (on) { g.setColour (theme::cyan.withAlpha (0.3f)); g.fillEllipse (led); }
        g.setColour (on ? theme::cyan : theme::faint);
        g.fillEllipse (led.reduced (4));
    }
}

void DiamondEditor::ModuleButton::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    if (param != nullptr && comingPhase == 0 && ledArea().expanded (6).contains (e.position))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (isOn() ? 0.0f : 1.0f);
        param->endChangeGesture();
        repaint();
        return;
    }
    if (onSelect) onSelect();
}

//==============================================================================
void DiamondEditor::Content::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Fond : dégradé profond + halos néon
    juce::ColourGradient bg (theme::bg2, r.getX(), r.getY(), theme::bg0, r.getRight(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();
    juce::ColourGradient o1 (theme::cyan.withAlpha (0.10f), 260, 120, juce::Colours::transparentBlack, 760, 120, true);
    g.setGradientFill (o1); g.fillRect (r);
    juce::ColourGradient o2 (theme::violet.withAlpha (0.12f), 1040, 640, juce::Colours::transparentBlack, 1040, 140, true);
    g.setGradientFill (o2); g.fillRect (r);

    // Grille fine façon hologramme
    g.setColour (juce::Colour (0x06ffffff));
    for (int x = 0; x < kW; x += 40) g.drawVerticalLine (x, 0, (float) kH);
    for (int y = 0; y < kH; y += 40) g.drawHorizontalLine (y, 0, (float) kW);

    // Logo
    theme::diamond (g, { 22, 20, 34, 28 });
    g.setColour (theme::text);
    g.setFont (theme::font (22.0f, true));
    g.drawText ("IFMY", juce::Rectangle<float> (66, 14, 70, 26), juce::Justification::centredLeft);
    g.setGradientFill (theme::neon ({ 66, 38, 190, 16 }));
    g.setFont (theme::font (12.5f, true));
    g.drawText ("D I A M O N D   P E R F E C T", juce::Rectangle<float> (66, 38, 220, 16), juce::Justification::centredLeft);

    // Ligne d'en-tête
    juce::ColourGradient line (juce::Colours::transparentBlack, 0, 72, theme::cyan.withAlpha (0.5f), r.getCentreX(), 72, false);
    line.addColour (1.0, juce::Colours::transparentBlack);
    g.setGradientFill (line);
    g.fillRect (0.0f, 71.0f, r.getWidth(), 1.0f);

    // Panneau vumètres
    theme::glassPanel (g, { 1012, 86, 152, 614 }, 14.0f);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f, true));
    g.drawText ("CPU", juce::Rectangle<float> (1012, 650, 152, 16), juce::Justification::centred);
    const float cpu = ed.cpuShown;
    g.setColour (cpu > 0.7f ? theme::hot : (cpu > 0.4f ? theme::warn : theme::cyan));
    g.setFont (theme::font (15.0f, true));
    g.drawText (juce::String (juce::roundToInt (cpu * 100.0f)) + " %", juce::Rectangle<float> (1012, 668, 152, 20), juce::Justification::centred);

    // Pied de page
    g.setColour (theme::faint);
    g.setFont (theme::font (11.0f));
    g.drawText (juce::String ("v") + JucePlugin_VersionString + (DiamondProcessor::isMidiVersion() ? "  ·  MIDI" : ""),
                juce::Rectangle<float> (22, 702, 300, 14), juce::Justification::centredLeft);
}

//==============================================================================
DiamondEditor::DiamondEditor (DiamondProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // --- En-tête ----------------------------------------------------------------
    auto setup = [this] (juce::TextButton& b, const juce::String& text, const juce::String& tip)
    {
        b.setButtonText (text);
        b.setTooltip (tip);
        content.addAndMakeVisible (b);
    };
    setup (prevBtn,  T ("◀"), T ("Preset précédent"));
    setup (nextBtn,  T ("▶"), T ("Preset suivant"));
    setup (presetBtn, {}, T ("Parcourir les presets"));
    setup (favBtn,   T ("★"), T ("Ajouter / retirer des favoris"));
    setup (saveBtn,  T ("SAVE"), T ("Enregistrer mon preset"));
    setup (moreBtn,  T ("•••"), T ("Renommer, supprimer, importer, exporter…"));
    setup (aBtn,     "A", T ("Réglage A"));
    setup (bBtn,     "B", T ("Réglage B (comparaison A/B)"));
    setup (undoBtn,  T ("↶"), T ("Annuler"));
    setup (redoBtn,  T ("↷"), T ("Rétablir"));
    setup (expertBtn, "EXPERT", T ("Mode expert : tous les réglages"));
    setup (bypassBtn, "BYPASS", T ("Bypass (sans clic)"));

    bypassBtn.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::bypass, bypassBtn);
    expertBtn.setClickingTogglesState (true);

    prevBtn.onClick   = [this] { proc.presets.next (-1); refreshPresetLabel(); };
    nextBtn.onClick   = [this] { proc.presets.next (+1); refreshPresetLabel(); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    favBtn.onClick    = [this] { proc.presets.toggleFavourite (proc.presets.currentName()); refreshPresetLabel(); };
    saveBtn.onClick   = [this] { savePresetDialog(); };
    moreBtn.onClick   = [this] { showMoreMenu(); };
    aBtn.onClick      = [this] { if (proc.presets.abSlot() != 0) proc.presets.switchAB(); refreshPresetLabel(); };
    bBtn.onClick      = [this] { if (proc.presets.abSlot() != 1) proc.presets.switchAB(); refreshPresetLabel(); };
    undoBtn.onClick   = [this] { proc.presets.undo(); };
    redoBtn.onClick   = [this] { proc.presets.redo(); };
    expertBtn.onClick = [this] { setExpert (expertBtn.getToggleState()); };

    // --- Rack + pages -------------------------------------------------------------
    for (int i = 0; i < (int) std::size (kModules); ++i)
    {
        const auto& d = kModules[i];
        auto* m = modules.add (new ModuleButton (d.name, T (d.sub),
                                                 d.power != nullptr ? proc.apvts.getParameter (d.power) : nullptr, d.phase));
        m->onSelect = [this, i] { selectModule (i); };
        content.addAndMakeVisible (m);
    }

    pages.add (new ui::TunePage<DiamondProcessor> (proc));
    pages.add (new ui::ComingSoon ("GRAPH", 6, { T ("Édition note par note (pitch, drift, vibrato, formant)"),
                                                 T ("Dessin libre de la courbe de justesse"),
                                                 T ("Correction du timing et du volume par note"),
                                                 T ("Puis intégration ARA2 dans les régions Logic") }));
    pages.add (new ui::ComingSoon ("BENDER", 5, { T ("Pitch ±24 demi-tons et formant indépendants"),
                                                  T ("Pad X/Y pour jouer la voix en live"),
                                                  T ("Démon, chipmunk, robot, alien, whisper, octaver"),
                                                  T ("Doubleur 2 / 4 voix") }));
    pages.add (new ui::ComingSoon ("HARMONY", 4, { T ("4 voix d'harmonie toujours dans ta tonalité"),
                                                   T ("15 styles : joyeux, triste, tendu, épique, jazzy, soul…"),
                                                   T ("Tierce, quinte, octave, sixte, septième au choix"),
                                                   T ("Mode chœur jusqu'à 32 voix + pilotage MIDI") }));
    pages.add (new ui::ComingSoon ("VOCODER", 5, { T ("Vocodeur 70s, moderne, talkbox, choral"),
                                                   T ("Porteuse interne (saw, supersaw, pad) ou clavier MIDI"),
                                                   T ("Voix synthétiques mélangeables"),
                                                   T ("Ta voix pilote basses, pads et leads") }));
    pages.add (new ui::DynamicsPage (proc));
    pages.add (new ui::EqPage (proc));
    pages.add (new ui::FxPage (proc));
    pages.add (new ui::MasterPage<DiamondProcessor> (proc));
    for (auto* pg : pages) content.addChildComponent (pg);

    for (auto* m : { &inMeter, &outMeter, &grMeter, &dsMeter }) content.addAndMakeVisible (m);

    // --- Taille (redimensionnable, proportions fixes) ---------------------------------
    setResizable (true, true);
    setResizeLimits (kW * 6 / 10, kH * 6 / 10, kW * 16 / 10, kH * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);

    const double scale = juce::jlimit (0.6, 1.6, (double) proc.uiState.getProperty ("scale", 1.0));
    expertBtn.setToggleState ((bool) proc.uiState.getProperty ("expert", false), juce::dontSendNotification);
    setExpert (expertBtn.getToggleState());
    selectModule (juce::jlimit (0, pages.size() - 1, (int) proc.uiState.getProperty ("module", 0)));
    layoutContent();
    setSize (juce::roundToInt (kW * scale), juce::roundToInt (kH * scale));

    refreshPresetLabel();
    startTimerHz (30);
}

DiamondEditor::~DiamondEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void DiamondEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kW, kH);
    proc.uiState.setProperty ("scale", scale, nullptr);
}

void DiamondEditor::layoutContent()
{
    // En-tête
    int x = 330;
    auto place = [&x] (juce::Component& c, int w) { c.setBounds (x, 18, w, 36); x += w + 6; };
    place (prevBtn, 36); place (presetBtn, 280); place (nextBtn, 36); x += 6;
    place (favBtn, 36); place (saveBtn, 56); place (moreBtn, 42);

    x = 864;
    place (aBtn, 30); place (bBtn, 30); x += 6;
    place (undoBtn, 34); place (redoBtn, 34);
    expertBtn.setBounds (1022, 18, 66, 36);
    bypassBtn.setBounds (1094, 18, 70, 36);

    // Rack
    const int rackTop = 86, rackH = 614, gap = 6, n = modules.size();
    const int bh = (rackH - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i) modules[i]->setBounds (20, rackTop + i * (bh + gap), 190, bh);

    // Pages
    for (auto* pg : pages) pg->setBounds (226, 86, 770, 614);

    // Vumètres
    const int mx = 1022, mw = 33;
    inMeter.setBounds (mx, 100, mw, 540);
    outMeter.setBounds (mx + mw, 100, mw, 540);
    grMeter.setBounds (mx + mw * 2, 100, mw, 540);
    dsMeter.setBounds (mx + mw * 3, 100, mw, 540);
}

void DiamondEditor::selectModule (int index)
{
    currentModule = index;
    for (int i = 0; i < pages.size(); ++i) pages[i]->setVisible (i == index);
    for (int i = 0; i < modules.size(); ++i) { modules[i]->selected = (i == index); modules[i]->repaint(); }
    proc.uiState.setProperty ("module", index, nullptr);
}

void DiamondEditor::setExpert (bool e)
{
    proc.uiState.setProperty ("expert", e, nullptr);
    for (auto* pg : pages)
        if (auto* page = dynamic_cast<ui::Page*> (pg)) page->setExpert (e);
}

//==============================================================================
void DiamondEditor::timerCallback()
{
    ++tickCount;

    if (auto* page = dynamic_cast<ui::Page*> (pages[currentModule])) page->tick();

    inMeter.setLevel (proc.inPeak.exchange (0.0f));
    outMeter.setLevel (proc.outPeak.exchange (0.0f));
    grMeter.setLevel (proc.compGr.load());
    dsMeter.setLevel (proc.deessGr.load());

    for (auto* m : modules)
        if (m->isOn() != m->lastOn) { m->lastOn = m->isOn(); m->repaint(); }

    if (tickCount % 3 == 0)
    {
        proc.presets.poll();
        undoBtn.setEnabled (proc.presets.canUndo());
        redoBtn.setEnabled (proc.presets.canRedo());
    }

    if (tickCount % 15 == 0)
    {
        cpuShown = proc.cpuLoad.load();
        content.repaint (1012, 646, 152, 46);
        refreshPresetLabel();
    }
}

void DiamondEditor::refreshPresetLabel()
{
    auto& pm = proc.presets;
    presetBtn.setButtonText (pm.currentCategory().toUpperCase() + T ("  ·  ") + pm.currentName());
    const bool fav = pm.isFavourite (pm.currentName());
    favBtn.setColour (juce::TextButton::textColourOffId, fav ? theme::warn : theme::dim);
    aBtn.setToggleState (pm.abSlot() == 0, juce::dontSendNotification);
    bBtn.setToggleState (pm.abSlot() == 1, juce::dontSendNotification);
}

//==============================================================================
void DiamondEditor::showPresetMenu()
{
    auto& pm = proc.presets;
    pm.refresh();
    const auto& all = pm.all();

    juce::PopupMenu menu, favs;
    for (int i = 0; i < all.size(); ++i)
        if (pm.isFavourite (all[i].name))
            favs.addItem (i + 1, all[i].name, true, i == pm.currentIndex());
    if (favs.getNumItems() > 0) { menu.addSubMenu (T ("★  Favoris"), favs); menu.addSeparator(); }

    menu.addSectionHeader (T ("PRESETS D'USINE"));
    for (auto& cat : pm.categories())
    {
        juce::PopupMenu sub;
        bool any = false;
        for (int i = 0; i < all.size(); ++i)
            if (all[i].factory && all[i].category == cat) { sub.addItem (i + 1, all[i].name, true, i == pm.currentIndex()); any = true; }
        if (any) menu.addSubMenu (cat, sub);
    }

    menu.addSeparator();
    menu.addSectionHeader (T ("MES PRESETS"));
    bool anyUser = false;
    for (auto& cat : pm.categories())
    {
        juce::PopupMenu sub;
        bool any = false;
        for (int i = 0; i < all.size(); ++i)
            if (! all[i].factory && all[i].category == cat) { sub.addItem (i + 1, all[i].name, true, i == pm.currentIndex()); any = true; }
        if (any) { menu.addSubMenu (cat, sub); anyUser = true; }
    }
    if (! anyUser) menu.addItem (-1, T ("(aucun pour l'instant : clique sur SAVE)"), false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn).withMinimumWidth (presetBtn.getWidth()),
                        [this] (int r) { if (r > 0) { proc.presets.load (r - 1); refreshPresetLabel(); } });
}

void DiamondEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow (T ("Enregistrer un preset"), T ("Donne un nom à ton réglage :"), juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    const auto& pm = proc.presets;
    const bool isUser = juce::isPositiveAndBelow (pm.currentIndex(), pm.all().size()) && ! pm.all()[pm.currentIndex()].factory;
    w->addTextEditor ("name", isUser ? pm.currentName() : pm.currentName() + " (IFMY)", T ("Nom"));
    w->addTextEditor ("cat", isUser ? pm.currentCategory() : "Mes presets", T ("Catégorie"));
    w->addButton (T ("Enregistrer"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (T ("Annuler"), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<DiamondEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name");
        const auto cat = w->getTextEditorContents ("cat");
        if (! safe->proc.presets.save (name, cat))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, T ("Impossible d'enregistrer"),
                T ("Choisis un nom non vide, différent des presets d'usine."));
        safe->refreshPresetLabel();
    }), true);
}

void DiamondEditor::renameDialog()
{
    auto& pm = proc.presets;
    const int idx = pm.currentIndex();
    if (! juce::isPositiveAndBelow (idx, pm.all().size()) || pm.all()[idx].factory)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, T ("Renommer"),
            T ("Les presets d'usine ne se renomment pas. Enregistre d'abord ta version avec SAVE."));
        return;
    }
    auto* w = new juce::AlertWindow (T ("Renommer"), T ("Nouveau nom :"), juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", pm.currentName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (T ("Annuler"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DiamondEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, idx] (int r)
    {
        if (r == 1 && safe != nullptr && ! safe->proc.presets.rename (idx, w->getTextEditorContents ("name")))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, T ("Renommer"), T ("Ce nom est vide ou déjà pris."));
        if (safe != nullptr) safe->refreshPresetLabel();
    }), true);
}

void DiamondEditor::showMoreMenu()
{
    auto& pm = proc.presets;
    const int idx = pm.currentIndex();
    const bool isUser = juce::isPositiveAndBelow (idx, pm.all().size()) && ! pm.all()[idx].factory;

    juce::PopupMenu m;
    m.addItem (1, T ("Renommer le preset…"), isUser);
    m.addItem (2, T ("Supprimer le preset"), isUser);
    m.addSeparator();
    m.addItem (3, T ("Importer un preset…"));
    m.addItem (4, T ("Exporter le réglage actuel…"));
    m.addItem (5, T ("Ouvrir le dossier des presets"));
    m.addSeparator();
    m.addItem (6, T ("Garder ma tonalité en changeant de preset"), true, pm.keepKey);
    m.addItem (7, T ("Copier le réglage actuel vers l'autre slot A/B"));

    juce::Component::SafePointer<DiamondEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (moreBtn), [safe, idx] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc.presets;
        switch (r)
        {
            case 1: safe->renameDialog(); break;
            case 2:
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, T ("Supprimer"),
                    T ("Supprimer « ") + p.currentName() + T (" » ? (il part dans la Corbeille)"), T ("Supprimer"), T ("Annuler"), safe.getComponent(),
                    juce::ModalCallbackFunction::create ([safe, idx] (int ok)
                    {
                        if (ok == 1 && safe != nullptr) { safe->proc.presets.remove (idx); safe->proc.presets.next (0); safe->refreshPresetLabel(); }
                    }));
                break;
            case 3:
                safe->chooser = std::make_unique<juce::FileChooser> (T ("Importer un preset"), juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*" + PresetManager::extension);
                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
                {
                    if (safe != nullptr && fc.getResult().existsAsFile()) safe->proc.presets.importFile (fc.getResult());
                });
                break;
            case 4:
                safe->chooser = std::make_unique<juce::FileChooser> (T ("Exporter le preset"),
                    juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (p.currentName() + PresetManager::extension), "*" + PresetManager::extension);
                safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe] (const juce::FileChooser& fc)
                {
                    if (safe != nullptr && fc.getResult() != juce::File()) safe->proc.presets.exportCurrent (fc.getResult());
                });
                break;
            case 5: PresetManager::presetDir().revealToUser(); break;
            case 6: p.keepKey = ! p.keepKey; break;
            case 7: p.copyAtoB(); break;
            default: break;
        }
    });
}
