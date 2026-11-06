#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties DiamondProcessor::makeBuses()
{
    auto b = juce::AudioProcessor::BusesProperties()
                 .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #if ! DIAMOND_MIDI
    // Side-chain optionnel (Unmask / ducking, Phase 3). Pas dans la version MIDI : dans Logic,
    // le menu Side Chain d'un « MIDI-controlled Effect » alimente l'entrée principale.
    b = b.withInput ("Sidechain", juce::AudioChannelSet::stereo(), false);
   #endif
    return b;
}

DiamondProcessor::DiamondProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "DIAMOND", params::createLayout()),
      presets (apvts)
{
    cacheParams();
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));
    uiState.setProperty ("expert", false, nullptr);
    uiState.setProperty ("module", 0, nullptr);
    uiState.setProperty ("scale", 1.0, nullptr);

    // Nouvelle instance = preset « IFMY Signature » (un projet Logic existant écrase ensuite ces valeurs)
    presets.loadFactory (0);
}

void DiamondProcessor::cacheParams()
{
    auto g = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    p = { g (pid::bypass), g (pid::inGain), g (pid::outGain), g (pid::safety),
          g (pid::tuneOn), g (pid::key), g (pid::scale), g (pid::retune), g (pid::amount), g (pid::humanize),
          g (pid::transpose), g (pid::refHz), g (pid::midiForce),
          g (pid::dynOn), g (pid::gateOn), g (pid::gateThr), g (pid::dsOn), g (pid::dsFreq), g (pid::dsThr),
          g (pid::cmpOn), g (pid::cmpThr), g (pid::cmpRatio), g (pid::cmpAtt), g (pid::cmpRel), g (pid::cmpGain),
          g (pid::satOn), g (pid::satDrive), g (pid::satMix),
          g (pid::eqOn), g (pid::eqHpf), g (pid::eqMud), g (pid::eqPres), g (pid::eqAir),
          g (pid::fxOn), g (pid::dblOn), g (pid::dblMix), g (pid::dblWidth), g (pid::dlyOn), g (pid::dlyDiv),
          g (pid::dlyFb), g (pid::dlyTone), g (pid::dlyMix), g (pid::dlyPing),
          g (pid::revOn), g (pid::revSize), g (pid::revDamp), g (pid::revWidth), g (pid::revMix) };
}

//==============================================================================
int DiamondProcessor::getNumPrograms() { return (int) PresetManager::factory().size(); }

int DiamondProcessor::getCurrentProgram()
{
    return juce::jmax (0, presets.currentFactoryIndex());
}

void DiamondProcessor::setCurrentProgram (int index)
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        presets.loadFactory (index);
    else
    {
        juce::WeakReference<DiamondProcessor> weak (this);
        juce::MessageManager::callAsync ([weak, index] { if (auto* self = weak.get()) self->presets.loadFactory (index); });
    }
}

const juce::String DiamondProcessor::getProgramName (int index)
{
    const auto& f = PresetManager::factory();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

//==============================================================================
bool DiamondProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();

    if (out != mono && out != stereo) return false;
    if (in != mono && in != stereo)   return false;
    if (in == stereo && out == mono)  return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != mono && sc != stereo) return false;
    }
    return true;
}

void DiamondProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);

    tuner.prepare (sr, 2);
    const int latency = tuner.getLatency();
    setLatencySamples (latency);
    for (auto& d : dryLine) d.prepare (latency + kChunk + 64);

    gate.prepare (sr);
    for (int c = 0; c < 2; ++c) { hpf[c].reset(); mud[c].reset(); pres[c].reset(); air[c].reset(); }
    for (auto& e : lastEq) e = -999.0f;
    deess.prepare (sr);
    comp.prepare (sr);
    dbl.prepare (sr);
    dly.prepare (sr);
    reverb.setSampleRate (sr);
    reverb.reset();
    revBuf.setSize (2, kChunk); // préalloué : jamais de réallocation dans processBlock

    inGainS.reset (sr, 0.03);  inGainS.setCurrentAndTargetValue (dsp::dbToGain (*p.inGain));
    outGainS.reset (sr, 0.03); outGainS.setCurrentAndTargetValue (dsp::dbToGain (*p.outGain));
    revMixS.reset (sr, 0.05);  revMixS.setCurrentAndTargetValue (0);
    wetS.reset (sr, 0.02);     wetS.setCurrentAndTargetValue (*p.bypass > 0.5f ? 0.0f : 1.0f);

    std::fill (std::begin (held), std::end (held), false);
}

void DiamondProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())                         held[msg.getNoteNumber()] = true;
        else if (msg.isNoteOff())                   held[msg.getNoteNumber()] = false;
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            std::fill (std::begin (held), std::end (held), false);
    }
}

void DiamondProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();

    auto main = getBusBuffer (buffer, true, 0);
    const int n = main.getNumSamples();
    const int numIn = getMainBusNumInputChannels(), numOut = getMainBusNumOutputChannels();
    if (n == 0 || numOut == 0) { midi.clear(); return; }

    if (numIn == 1 && numOut >= 2 && main.getNumChannels() >= 2) main.copyFrom (1, 0, main, 0, 0, n);
    const int nc = juce::jmin (2, numOut, main.getNumChannels());

    // --- MIDI : notes tenues = notes cibles de l'autotune ----------------------
    uint16_t midiMask = 0;
    if constexpr (DIAMOND_MIDI != 0)
    {
        handleMidi (midi);
        int count = 0;
        for (int i = 0; i < 128; ++i)
            if (held[i]) { midiMask = (uint16_t) (midiMask | (1u << (i % 12))); ++count; }
        midiNotesHeld.store (count, std::memory_order_relaxed);
        midiMaskUi.store (midiMask, std::memory_order_relaxed);
        if (*p.midiForce < 0.5f) midiMask = 0;
    }
    midi.clear();

    // Tranches de taille fixe : marche avec n'importe quelle taille de buffer envoyée par Logic
    for (int start = 0; start < n; start += kChunk)
    {
        float* chans[2] = { main.getWritePointer (0) + start, nc > 1 ? main.getWritePointer (1) + start : nullptr };
        processChunk (chans, nc, juce::jmin (kChunk, n - start), midiMask);
    }

    for (int c = nc; c < main.getNumChannels(); ++c) main.clear (c, 0, n);

    // Charge CPU = temps de calcul / durée du bloc
    const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const float load = (float) (elapsed / (n / sr));
    cpuLoad.store (cpuLoad.load() * 0.9f + 0.1f * load);
}

void DiamondProcessor::processChunk (float** ch, int nc, int n, uint16_t midiMask)
{
    // --- Paramètres --------------------------------------------------------------
    dsp::AutoTune::Params tp;
    tp.on        = *p.tuneOn > 0.5f;
    tp.key       = juce::jlimit (0, 11, (int) *p.key);
    tp.scale     = dsp::kScales[juce::jlimit (0, dsp::kNumScales - 1, (int) *p.scale)].mask;
    tp.retuneMs  = *p.retune;
    tp.amount    = *p.amount * 0.01f;
    tp.humanize  = *p.humanize * 0.01f;
    tp.transpose = std::round (p.transpose->load());
    tp.refHz     = *p.refHz;
    tp.midiMask  = midiMask;
    tuner.setParams (tp);

    const bool dynOn = *p.dynOn > 0.5f, fxOn = *p.fxOn > 0.5f;
    const bool gateOn = dynOn && *p.gateOn > 0.5f, dsOn = dynOn && *p.dsOn > 0.5f,
               cmpOn = dynOn && *p.cmpOn > 0.5f, satOn = dynOn && *p.satOn > 0.5f,
               eqOn = *p.eqOn > 0.5f,
               dblOn = fxOn && *p.dblOn > 0.5f, dlyOn = fxOn && *p.dlyOn > 0.5f, revOn = fxOn && *p.revOn > 0.5f,
               ping = *p.dlyPing > 0.5f, safety = *p.safety > 0.5f;

    gate.setThreshold (*p.gateThr);

    const float eqv[4] = { *p.eqHpf, *p.eqMud, *p.eqPres, *p.eqAir };
    if (std::memcmp (eqv, lastEq, sizeof (eqv)) != 0)
    {
        for (int c = 0; c < 2; ++c)
        {
            hpf[c].highpass (sr, eqv[0], 0.7071);
            mud[c].peak (sr, 300.0, 1.0, eqv[1]);
            pres[c].peak (sr, 3500.0, 0.8, eqv[2]);
            air[c].highShelf (sr, 12000.0, eqv[3]);
        }
        std::copy (eqv, eqv + 4, lastEq);
    }

    deess.setFreq (*p.dsFreq);
    deess.setThreshold (*p.dsThr);
    comp.set (*p.cmpThr, *p.cmpRatio, *p.cmpAtt, *p.cmpRel, *p.cmpGain);
    sat.set (*p.satDrive, *p.satMix * 0.01f);

    const float dblMix = *p.dblMix * 0.01f, dblWidth = *p.dblWidth * 0.01f;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0 && *b < 999.0) bpm = *b;
    const int div = juce::jlimit (0, 7, (int) *p.dlyDiv);
    const float dlySamples = (float) (60.0 / bpm * params::kDivBeats[div] * sr);
    const float dlyFb = *p.dlyFb * 0.01f, dlyMix = *p.dlyMix * 0.01f;
    const float dlyTone = (float) (1.0 - std::exp (-2.0 * dsp::kPiD * *p.dlyTone / sr));

    inGainS.setTargetValue (dsp::dbToGain (*p.inGain));
    outGainS.setTargetValue (dsp::dbToGain (*p.outGain));
    wetS.setTargetValue (*p.bypass > 0.5f ? 0.0f : 1.0f);

    // --- Chaîne, échantillon par échantillon -----------------------------------
    float inPk = 0;
    const int latency = tuner.getLatency();

    for (int i = 0; i < n; ++i)
    {
        float f[2] = { ch[0][i], nc > 1 ? ch[1][i] : 0.0f };
        for (int c = 0; c < nc; ++c) dryLine[c].push (f[c]);

        const float g = inGainS.getNextValue();
        for (int c = 0; c < nc; ++c) { f[c] *= g; inPk = std::max (inPk, std::abs (f[c])); }

        if (gateOn) gate.process (f, nc);
        tuner.process (f, nc);

        if (eqOn)
            for (int c = 0; c < nc; ++c)
                f[c] = air[c].process (pres[c].process (mud[c].process (hpf[c].process (f[c]))));

        if (dsOn)  deess.process (f, nc); else deess.grDb = 0;
        if (cmpOn) comp.process (f, nc);  else comp.grDb = 0;
        if (satOn) sat.process (f, nc);
        if (dblOn) dbl.process (f, nc, dblMix, dblWidth);
        if (dlyOn) dly.process (f, nc, dlySamples, dlyFb, dlyTone, dlyMix, ping);

        ch[0][i] = f[0];
        if (nc > 1) ch[1][i] = f[1];
    }

    // --- Réverbe (buffer préalloué, traité par tranches) ----------------------------
    revMixS.setTargetValue (revOn ? *p.revMix * 0.01f : 0.0f);
    if (revOn || revMixS.isSmoothing() || revMixS.getCurrentValue() > 0.0001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * (*p.revSize * 0.01f);
        rp.damping  = *p.revDamp * 0.01f;
        rp.width    = *p.revWidth * 0.01f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);

        for (int start = 0; start < n; start += revBuf.getNumSamples())
        {
            const int len = juce::jmin (revBuf.getNumSamples(), n - start);
            for (int c = 0; c < nc; ++c) revBuf.copyFrom (c, 0, ch[c] + start, len);
            if (nc > 1) reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), len);
            else        reverb.processMono   (revBuf.getWritePointer (0), len);

            for (int i = 0; i < len; ++i)
            {
                const float m = revMixS.getNextValue() * 0.35f;
                for (int c = 0; c < nc; ++c) ch[c][start + i] += m * revBuf.getSample (c, i);
            }
        }
    }
    else
    {
        reverb.reset();
    }

    // --- Sortie : gain, bypass sans clic, sécurité anti-NaN et soft-clip --------------
    float outPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainS.getNextValue();
        const float w = wetS.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            const float dry = dryLine[c].readInt (n - 1 - i + latency);
            float y = ch[c][i] * g;
            if (! std::isfinite (y)) y = 0.0f;
            if (safety)
            {
                const float a = std::abs (y);
                if (a > 0.9f) y = std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), y);
            }
            y = dry + w * (y - dry);
            ch[c][i] = y;
            outPk = std::max (outPk, std::abs (y));
        }
    }

    inPeak.store (std::max (inPeak.load(), inPk));
    outPeak.store (std::max (outPeak.load(), outPk));
    compGr.store (comp.grDb);
    deessGr.store (deess.grDb);
}

//==============================================================================
void DiamondProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    state.setProperty ("presetName", presets.currentName(), nullptr);
    state.setProperty ("presetCategory", presets.currentCategory(), nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DiamondProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (auto ui = state.getChildWithName ("UI"); ui.isValid())
    {
        uiState.copyPropertiesFrom (ui, nullptr);
        state.removeChild (ui, nullptr);
    }

    const auto name = state.getProperty ("presetName", "IFMY Signature").toString();
    const auto cat  = state.getProperty ("presetCategory", "Signature").toString();

    // replaceState garde les valeurs par défaut pour les paramètres absents
    // (projets sauvegardés avec une ancienne version du plugin)
    apvts.replaceState (state);

    // Sécurité : on force chaque paramètre à la valeur exacte sauvegardée
    // (replaceState peut ignorer un interrupteur dont la valeur interne n'était pas pile 0 ou 1)
    for (const auto& child : state)
    {
        if (! child.hasType ("PARAM")) continue;
        if (auto* rp = apvts.getParameter (child.getProperty ("id").toString()))
        {
            const float v = rp->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (rp->getValue() - v) > 1.0e-6f) rp->setValueNotifyingHost (v);
        }
    }

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        presets.setCurrentName (name, cat);
    else
    {
        juce::WeakReference<DiamondProcessor> weak (this);
        juce::MessageManager::callAsync ([weak, name, cat] { if (auto* self = weak.get()) self->presets.setCurrentName (name, cat); });
    }
}

juce::AudioProcessorEditor* DiamondProcessor::createEditor() { return new DiamondEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DiamondProcessor(); }
