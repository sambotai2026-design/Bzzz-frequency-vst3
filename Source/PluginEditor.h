#pragma once
#include "PluginProcessor.h"

namespace ui
{
// teintes par style (identiques au web) et themes
static const float kStyleHue[6] = { 18, 200, 82, 268, 348, 162 };
struct ThemeCols { juce::uint32 bg0, bg1; };
static const ThemeCols kThemes[3] = {
    { 0xff050508, 0xff0B0B12 },   // classic
    { 0xff05060F, 0xff0B0D1F },   // nebula
    { 0xff0A0806, 0xff141008 } }; // carbon

juce::Colour hsl (float h, float s, float l, float a = 1.0f);

// ---- pastille style web ----
class Chip : public juce::Button
{
public:
    Chip (const juce::String& t) : juce::Button (t) {}
    std::function<float()> hue = []{ return 18.0f; };
    bool small = false;
    void paintButton (juce::Graphics&, bool, bool) override;
};

// ---- knob style web : anneau + valeur + libelle ----
class Knob : public juce::Component
{
public:
    juce::String label;
    float mn = 0, mx = 1; bool logScale = false;
    std::function<float()> getV; std::function<void(float)> setV;
    std::function<juce::String(float)> fmt = [] (float v) { return juce::String (v, 2); };
    std::function<float()> hue = []{ return 18.0f; };
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { dy = e.y; sp = toP (getV()); }
    void mouseDrag (const juce::MouseEvent& e) override
    { setV (toV (juce::jlimit (0.f, 1.f, sp + (dy - e.y) / 170.0f))); repaint(); }
private:
    int dy = 0; float sp = 0;
    float toP (float v) const
    { return logScale ? std::log (juce::jmax (v, mn) / mn) / std::log (mx / mn) : (v - mn) / (mx - mn); }
    float toV (float p) const
    { return logScale ? mn * std::pow (mx / mn, p) : mn + p * (mx - mn); }
};

// ---- reacteur : replique du canvas web, nourri par le spectre reel ----
class Reactor : public juce::Component, private juce::Timer
{
public:
    explicit Reactor (BzzzProcessor& p);
    std::function<float()> hue = []{ return 18.0f; };
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    BzzzProcessor& proc;
    float bins [96] {};
    float rot = 0, low = 0, mid = 0, kick = 0;
};
}

class BzzzEditor;

// page interne a taille fixe (1380 de large comme le web), zoomee par la fenetre
class BzzzPage : public juce::Component, private juce::Timer
{
public:
    BzzzPage (BzzzProcessor&, BzzzEditor&);
    static constexpr int W = 1380, H = 2210;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    float hue() const { return ui::kStyleHue[juce::jlimit (0, 5, proc.styleIdx.load())]; }
    int   theme = 0;

private:
    void timerCallback() override;
    void refreshChips();
    void refreshInspector();
    void selectTrack (int);
    void rebuildSongUI();
    void panel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    BzzzProcessor& proc;
    BzzzEditor& editor;

    // header
    std::unique_ptr<ui::Chip> styleChips[6], themeChips[3];

    // reacteur + macros
    ui::Reactor reactor;
    ui::Knob mCut, mRes, mSC, mDrv, mDly, mRev, mVol, mSwing;

    // pads
    std::unique_ptr<ui::Chip> pads[12], padMutes[12];
    int selTrack = 0;

    // inspecteur
    std::unique_ptr<ui::Chip> fTypes[6];
    ui::Knob ckCut, ckRes, ckDrv, ckCr, ckDe, ckPi, ckPa, ckSD, ckSR, ckVo;

    // sequenceur
    std::unique_ptr<ui::Chip> slotChips[8], copyChip, styChip, rndChip, clrChip,
                              songOnChip, songAddChip, songClrChip;
    juce::OwnedArray<ui::Chip> songEntries, songDels;
    bool copyArm = false;
    juce::Rectangle<int> gridArea;

    // synth
    std::unique_ptr<ui::Chip> synPwrChip, osc1C[4], osc2C[4], modeC[6], lfoC[3], octDn, octUp;
    ui::Knob sMix, sO2, sDet, sUni, sFM, sSub, sNz, sGl;
    ui::Knob sCut, sRes, sEnv, sFDec, sDrv, sCr;
    ui::Knob sA, sD, sS, sR, sLfoR, sLfoA, sLfoP;
    ui::Knob sW, sVol, sSD, sSR;
    juce::Rectangle<int> kbdArea, rollArea, accRow, sldRow;
    int kbdHeld = -1;
    void kbdEvent (const juce::MouseEvent&, bool down);
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    // navigateurs kits/patches
    juce::ComboBox kitBox, patBox;
    std::unique_ptr<ui::Chip> kitPrev, kitNext, kitRnd, patPrev, patNext, patRnd;

    int uiStep = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BzzzPage)
};

class BzzzEditor : public juce::AudioProcessorEditor
{
public:
    explicit BzzzEditor (BzzzProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff050508)); }
private:
    BzzzProcessor& proc;
    juce::Viewport view;
    juce::Component holder;
    BzzzPage page;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BzzzEditor)
};
