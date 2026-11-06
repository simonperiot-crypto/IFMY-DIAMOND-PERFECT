#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"
#include "PresetManager.h"
#include "dsp/AutoTune.h"
#include "dsp/Effects.h"

#ifndef DIAMOND_MIDI
 #define DIAMOND_MIDI 0
#endif

class DiamondProcessor : public juce::AudioProcessor
{
public:
    DiamondProcessor();
    ~DiamondProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return DIAMOND_MIDI ? "IFMY Diamond Perfect MIDI" : "IFMY Diamond Perfect"; }
    bool acceptsMidi() const override  { return DIAMOND_MIDI != 0; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // Réglages d'interface sauvegardés avec le projet (pas des paramètres audio)
    juce::ValueTree uiState { "UI" };

    // Mesures pour l'interface (lecture sans verrou)
    std::atomic<float> inPeak { 0 }, outPeak { 0 }, compGr { 0 }, deessGr { 0 }, cpuLoad { 0 };
    std::atomic<int>   midiNotesHeld { 0 };
    std::atomic<uint32_t> midiMaskUi { 0 };
    dsp::AutoTune tuner;

    static constexpr bool isMidiVersion() { return DIAMOND_MIDI != 0; }

private:
    struct P // pointeurs vers les valeurs brutes (aucune recherche par nom dans le thread audio)
    {
        std::atomic<float> *bypass, *inGain, *outGain, *safety,
            *tuneOn, *key, *scale, *retune, *amount, *humanize, *transpose, *refHz, *midiForce,
            *dynOn, *gateOn, *gateThr, *dsOn, *dsFreq, *dsThr, *cmpOn, *cmpThr, *cmpRatio, *cmpAtt, *cmpRel, *cmpGain,
            *satOn, *satDrive, *satMix,
            *eqOn, *eqHpf, *eqMud, *eqPres, *eqAir,
            *fxOn, *dblOn, *dblMix, *dblWidth, *dlyOn, *dlyDiv, *dlyFb, *dlyTone, *dlyMix, *dlyPing,
            *revOn, *revSize, *revDamp, *revWidth, *revMix;
    } p {};

    static BusesProperties makeBuses();
    void cacheParams();
    void handleMidi (const juce::MidiBuffer& midi);
    void processChunk (float** ch, int nc, int n, uint16_t midiMask);
    static constexpr int kChunk = 1024;

    juce::AudioParameterBool* bypassParam = nullptr;

    double sr = 48000;
    int maxBlock = 512;

    dsp::Gate gate;
    dsp::Biquad hpf[2], mud[2], pres[2], air[2];
    dsp::DeEsser deess;
    dsp::Compressor comp;
    dsp::Saturator sat;
    dsp::Doubler dbl;
    dsp::StereoDelay dly;
    juce::Reverb reverb;
    juce::AudioBuffer<float> revBuf;
    dsp::DelayLine dryLine[2];  // signal sec aligné sur la latence (bypass sans clic)

    juce::SmoothedValue<float> inGainS, outGainS, revMixS, wetS;
    float lastEq[4] { -999, -999, -999, -999 };
    bool held[128] {};

    JUCE_DECLARE_WEAK_REFERENCEABLE (DiamondProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiamondProcessor)
};
