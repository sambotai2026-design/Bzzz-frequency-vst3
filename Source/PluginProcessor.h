#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DrumEngine.h"
#include "SynthEngine.h"
#include "Presets.h"

class BzzzProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kTracks = 12, kSteps = 16, kSlots = 8, kMaxSong = 32;

    BzzzProcessor();
    ~BzzzProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }
    const juce::String getName() const override      { return "B'Zzz Frequency"; }
    bool acceptsMidi() const override                { return true; }
    bool producesMidi() const override               { return false; }
    double getTailLengthSeconds() const override     { return 3.0; }
    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ================= etat partage UI <-> audio =================
    std::atomic<int> grid [kTracks][kSteps];             // 0/1/2
    std::atomic<int> rollNote [kSteps];                  // -100 = vide
    std::atomic<int> rollAcc [kSteps], rollSlide [kSteps];

    struct Slot { int g[kTracks][kSteps]; int n[kSteps]; int a[kSteps]; int s[kSteps]; };
    Slot  slots [kSlots];
    std::atomic<int> curSlot { 0 };

    std::atomic<int> songLen { 0 }, songOn { 0 }, songPos { 0 };
    std::atomic<int> songSlot [kMaxSong], songReps [kMaxSong];

    bzzz::ChannelCfg chCfg [kTracks];
    std::atomic<int> trackMute [kTracks];

    bzzz::SynCfg synCfg;
    std::atomic<int> synthPower { 1 };
    std::atomic<int> synOct { 2 };
    std::atomic<int> styleIdx { 0 };
    std::atomic<int> curKit { 0 }, curPatch { 0 };

    std::atomic<int> playStep { -1 };
    std::atomic<float> meterKick { 0 };
    std::atomic<float> hostBpm { 132.0f };

    // buffer circulaire pour le reacteur (spectre)
    static constexpr int kVisSize = 4096;
    float visRing [kVisSize] {};
    std::atomic<int> visWrite { 0 };

    // notes clavier UI -> thread audio
    void queueUiNote (int note, bool on);

    // taille de fenetre persistee
    std::atomic<int> edW { 1400 }, edH { 940 };

    void loadKit (int i);
    void loadPatch (int i);
    void applyStyleSeqs (int s);
    void clearPattern();
    void randomPattern();
    void storeCurToSlot();
    void gotoSlot (int i, bool store = true);
    void copySlotTo (int dest);
    void previewPad (int track);
    juce::CriticalSection cfgLock;
    juce::SpinLock uiNoteLock;
    std::vector<std::pair<int,bool>> uiNotes;

    juce::AudioProcessorValueTreeState apvts;

private:
    bzzz::Engine engine;
    bzzz::SynthEngine synth;
    bzzz::SVF masterF;
    juce::Reverb reverb;

    std::vector<float> dlyL, dlyR;
    int dlyPos = 0, dlyLenL = 12000, dlyLenR = 18000, dlyBuf = 0;

    double sr = 48000.0;
    int  lastStep = -1;
    bool wasPlaying = false;
    int  songBar = 0;
    float comp = 1.0f;

    std::atomic<float>* pCut{}, *pRes{}, *pDrv{}, *pVol{}, *pSwing{},
                      * pDlyMix{}, *pRev{}, *pSC{},
                      * pSCut{}, *pSRes{}, *pSEnv{}, *pSLfoR{}, *pSLfoA{}, *pSVol{};

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void fireStep (int stepMod);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BzzzProcessor)
};
