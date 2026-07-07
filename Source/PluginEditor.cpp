#include "PluginEditor.h"
using namespace bzzz;

static const char* kNames[12] = { "KICK","RUMBLE","SNARE","CLAP","CHH","OHH","RIDE","PERC","TOM","ACID","STAB","RISE" };
static const float kHues[12]  = { 355, 325, 30, 50, 90, 130, 170, 200, 230, 75, 265, 300 };
static const char* kStyleNm[6] = { "BERLIN","DETROIT","LONDON","PARIS","ROTTERDAM","TBILISI" };
static const char* kThemeNm[3] = { "CLASSIC","NEBULA","CARBON" };
static const char* kNoteNm[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

namespace ui
{
juce::Colour hsl (float h, float s, float l, float a)
{ return juce::Colour::fromHSL (h / 360.0f, s, l, a); }

// ----------------- Chip -----------------
void Chip::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    const float hu = hue();
    const bool on = getToggleState();
    const float r = b.getHeight() / 2;
    g.setColour (on ? hsl (hu, 1.f, .58f, .16f) : juce::Colour (0x09ffffff));
    g.fillRoundedRectangle (b, r);
    g.setColour (on ? hsl (hu, 1.f, .58f, .85f) : juce::Colour (over ? 0x30ffffff : 0x18ffffff));
    g.drawRoundedRectangle (b, r, 1.2f);
    if (on)
    { g.setColour (hsl (hu, 1.f, .58f, .30f)); g.drawRoundedRectangle (b.expanded (1.5f), r + 1.5f, 2.5f); }
    g.setColour (on ? hsl (hu, 1.f, .72f) : juce::Colour (0xff9A97AC));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), small ? 10.f : 11.f, juce::Font::bold)));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
}

// ----------------- Knob -----------------
void Knob::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    const float hu = hue();
    const float d = juce::jmin (full.getWidth(), full.getHeight() - 26.f);
    juce::Rectangle<float> k ((full.getWidth() - d) / 2, 2, d, d);
    const float p = toP (juce::jlimit (mn, mx, getV()));
    const float a0 = -2.42f, a1 = 2.42f, av = a0 + (a1 - a0) * p;
    // piste
    juce::Path track; track.addCentredArc (k.getCentreX(), k.getCentreY(), d/2 - 3, d/2 - 3, 0, a0, a1, true);
    g.setColour (juce::Colour (0x22ffffff));
    g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // valeur
    juce::Path val; val.addCentredArc (k.getCentreX(), k.getCentreY(), d/2 - 3, d/2 - 3, 0, a0, av, true);
    g.setColour (hsl (hu, 1.f, .58f));
    g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // lueur
    g.setColour (hsl (hu, 1.f, .58f, .25f));
    g.strokePath (val, juce::PathStrokeType (6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // centre
    g.setColour (juce::Colour (0xff0d0d13));
    g.fillEllipse (k.reduced (d * .22f));
    g.setColour (juce::Colour (0x14ffffff));
    g.drawEllipse (k.reduced (d * .22f), 1.f);
    // valeur texte
    g.setColour (juce::Colour (0xffF4F3F8));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.f, juce::Font::bold)));
    g.drawText (fmt (getV()), k.toNearestInt(), juce::Justification::centred);
    // libelle
    g.setColour (juce::Colour (0xff9A97AC));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain)));
    g.drawText (label.toUpperCase(), getLocalBounds().removeFromBottom (16), juce::Justification::centred);
}

// ----------------- Reactor -----------------
Reactor::Reactor (BzzzProcessor& p) : proc (p) { startTimerHz (30); setInterceptsMouseClicks (false, false); }

void Reactor::timerCallback()
{
    // copie des 1024 derniers echantillons
    float buf[1024];
    const int wp = proc.visWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < 1024; ++i)
        buf[i] = proc.visRing[(wp - 1024 + i + BzzzProcessor::kVisSize) & (BzzzProcessor::kVisSize - 1)];
    // Goertzel sur 96 bandes log 40 Hz -> 11 kHz
    const float sr = 48000.f;
    float lo = 0, mi = 0;
    for (int b = 0; b < 96; ++b)
    {
        const float f = 40.f * std::pow (11000.f / 40.f, b / 95.f);
        const float w = juce::MathConstants<float>::twoPi * f / sr;
        const float cw = 2.f * std::cos (w);
        float s0 = 0, s1 = 0, s2 = 0;
        for (int i = 0; i < 1024; i += 2)         // pas de 2 : suffisant et 2x moins cher
        { s0 = buf[i] + cw * s1 - s2; s2 = s1; s1 = s0; }
        float mag = std::sqrt (juce::jmax (0.f, s1*s1 + s2*s2 - cw*s1*s2)) / 220.f;
        mag = juce::jlimit (0.f, 1.f, mag);
        bins[b] += (mag - bins[b]) * (mag > bins[b] ? .55f : .22f);
        if (b < 10) lo += bins[b];
        if (b >= 20 && b < 80) mi += bins[b];
    }
    low += (lo / 10.f - low) * .4f;
    mid += (mi / 60.f - mid) * .4f;
    kick = proc.meterKick.load();
    rot += .0022f + mid * .004f;
    repaint();
}

void Reactor::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float cx = w / 2, cy = h / 2, sc = juce::jmin (w, h) / 500.f;
    const float hu = hue();
    const float R = (86.f + low * 26.f + kick * 22.f) * sc;
    // halo
    juce::ColourGradient grad (hsl (hu, 1.f, .6f, .22f + low * .4f + kick * .3f), cx, cy,
                               hsl (hu, 1.f, .6f, 0.f), cx + R * 2.4f, cy, true);
    g.setGradientFill (grad);
    g.fillEllipse (cx - R*2.4f, cy - R*2.4f, R*4.8f, R*4.8f);
    // barres radiales
    for (int i = 0; i < 96; ++i)
    {
        const float a = rot + (float) i / 96.f * juce::MathConstants<float>::twoPi;
        const float v = bins[i];
        const float r0 = R + 6*sc, r1 = R + (6 + v * 88) * sc;
        g.setColour (hsl (hu, 1.f, .58f + v * .22f, .35f + v * .65f));
        g.drawLine (cx + std::cos (a) * r0, cy + std::sin (a) * r0,
                    cx + std::cos (a) * r1, cy + std::sin (a) * r1, 3.4f * sc);
    }
    // coeur
    g.setColour (hsl (hu, 1.f, .5f + kick * .3f, .16f + kick * .5f));
    g.fillEllipse (cx - R*.62f, cy - R*.62f, R*1.24f, R*1.24f);
    g.setColour (hsl (hu, 1.f, .7f, .8f));
    g.drawEllipse (cx - R*.62f, cy - R*.62f, R*1.24f, R*1.24f, 2.f * sc);
    // anneau orbital
    juce::Path orb; orb.addCentredArc (cx, cy, R*1.55f, R*1.55f, rot * 2.f, .4f, 5.2f, true);
    g.setColour (hsl (hu, 1.f, .6f, .35f));
    g.strokePath (orb, juce::PathStrokeType (1.4f * sc));
}
} // namespace ui

// =====================================================================
//                              PAGE
// =====================================================================
static juce::String fmtHz (float v)
{ return v < 1000.f ? juce::String ((int) v) + "Hz" : juce::String (v / 1000.f, 1) + "k"; }
static juce::String fmtPc (float v) { return juce::String ((int) std::round (v * 100)) + "%"; }
static juce::String fmtMs (float v) { return juce::String ((int) std::round (v * 1000)) + "ms"; }

BzzzPage::BzzzPage (BzzzProcessor& p, BzzzEditor& e) : reactor (p), proc (p), editor (e)
{
    setSize (W, H);
    auto hueFn = [this] { return hue(); };
    reactor.hue = hueFn;
    addAndMakeVisible (reactor);

    auto mkChip = [this, hueFn] (std::unique_ptr<ui::Chip>& c, const juce::String& t, std::function<void()> fn, bool sm = false)
    { c = std::make_unique<ui::Chip> (t); c->hue = hueFn; c->small = sm; c->onClick = std::move (fn); addAndMakeVisible (*c); };

    // ---- header : styles + themes ----
    for (int s = 0; s < 6; ++s)
        mkChip (styleChips[s], kStyleNm[s], [this, s] { proc.applyStyleSeqs (s); refreshChips(); repaint(); });
    for (int t = 0; t < 3; ++t)
        mkChip (themeChips[t], kThemeNm[t], [this, t] { theme = t; refreshChips(); repaint(); });

    // ---- macros master (parametres hote, comme le web) ----
    auto paramKnob = [this, hueFn] (ui::Knob& k, const char* pid, const juce::String& lab,
                                    std::function<juce::String(float)> f)
    {
        auto* par = proc.apvts.getParameter (pid);
        const auto range = proc.apvts.getParameterRange (pid);
        k.label = lab; k.mn = range.start; k.mx = range.end; k.logScale = false; k.hue = hueFn; k.fmt = std::move (f);
        k.getV = [par, range] { return range.convertFrom0to1 (par->getValue()); };
        k.setV = [par, range] (float v) { par->setValueNotifyingHost (range.convertTo0to1 (v)); };
        addAndMakeVisible (k);
    };
    paramKnob (mCut, "cutoff", "Cutoff", fmtHz);  mCut.logScale = true;
    paramKnob (mRes, "res", "R\u00e9so", [] (float v) { return juce::String (v, 1); });
    paramKnob (mSC,  "sc", "Sidechain", fmtPc);
    paramKnob (mDrv, "drive", "Drive", fmtPc);
    paramKnob (mDly, "dlymix", "Delay", fmtPc);
    paramKnob (mRev, "revmix", "Space", fmtPc);
    paramKnob (mVol, "volume", "Volume", fmtPc);
    paramKnob (mSwing, "swing", "Swing", [] (float v) { return juce::String ((int) v) + "%"; });

    // ---- pads ----
    for (int i = 0; i < 12; ++i)
    {
        mkChip (pads[i], kNames[i], [this, i] { proc.previewPad (i); selectTrack (i); });
        pads[i]->hue = [i] { return kHues[i]; };
        mkChip (padMutes[i], "M", [this, i]
        { padMutes[i]->setToggleState (! padMutes[i]->getToggleState(), juce::dontSendNotification);
          proc.trackMute[i].store (padMutes[i]->getToggleState() ? 1 : 0); }, true);
        padMutes[i]->hue = [] { return 355.0f; };
    }

    // ---- inspecteur ----
    auto CH = [this]() -> bzzz::ChannelCfg& { return proc.chCfg[selTrack]; };
    auto chKnob = [this, CH] (ui::Knob& k, const juce::String& lab, float mn, float mx, bool lg,
                              std::function<juce::String(float)> f, std::function<float(const bzzz::ChannelCfg&)> gv,
                              std::function<void(bzzz::ChannelCfg&,float)> sv)
    {
        k.label = lab; k.mn = mn; k.mx = mx; k.logScale = lg; k.fmt = std::move (f);
        k.hue = [this] { return kHues[selTrack]; };
        k.getV = [CH, gv] { return gv (CH()); };
        k.setV = [this, CH, sv] (float v) { const juce::ScopedLock l (proc.cfgLock); sv (CH(), v); };
        addAndMakeVisible (k);
    };
    chKnob (ckCut, "Cutoff", 30, 18000, true, fmtHz, [](auto& c){return c.cutoff;}, [](auto& c,float v){c.cutoff=v;});
    chKnob (ckRes, "R\u00e9so", .05f, 24, true, [](float v){return juce::String(v,1);}, [](auto& c){return c.res;}, [](auto& c,float v){c.res=v;});
    chKnob (ckDrv, "Drive", 0, 1, false, fmtPc, [](auto& c){return c.drive;}, [](auto& c,float v){c.drive=v;});
    chKnob (ckCr,  "Crush", 0, 1, false, fmtPc, [](auto& c){return c.crush;}, [](auto& c,float v){c.crush=v;});
    chKnob (ckDe,  "Decay", .3f, 2.5f, false, [](float v){return juce::String(v,2)+"x";}, [](auto& c){return c.decay;}, [](auto& c,float v){c.decay=v;});
    chKnob (ckPi,  "Pitch", -12, 12, false, [](float v){return juce::String(v,1)+"st";}, [](auto& c){return c.pitch;}, [](auto& c,float v){c.pitch=v;});
    chKnob (ckPa,  "Pan", -1, 1, false, [](float v){return v<-.02f?"L"+juce::String((int)(-v*100)):(v>.02f?"R"+juce::String((int)(v*100)):juce::String("C"));}, [](auto& c){return c.pan;}, [](auto& c,float v){c.pan=v;});
    chKnob (ckSD,  "\u2192Delay", 0, 1, false, fmtPc, [](auto& c){return c.sendD;}, [](auto& c,float v){c.sendD=v;});
    chKnob (ckSR,  "\u2192Space", 0, 1, false, fmtPc, [](auto& c){return c.sendR;}, [](auto& c,float v){c.sendR=v;});
    chKnob (ckVo,  "Volume", 0, 1, false, fmtPc, [](auto& c){return c.vol;}, [](auto& c,float v){c.vol=v;});

    static const char* ftn[6] = { "OFF","LP","HP","BP","NOTCH","PEAK" };
    for (int i = 0; i < 6; ++i)
    {
        mkChip (fTypes[i], ftn[i], [this, i, CH]
        { { const juce::ScopedLock l (proc.cfgLock);
            if (i == 0) CH().fon = false; else { CH().fon = true; CH().ftype = i - 1; } }
          refreshInspector(); }, true);
        fTypes[i]->hue = [this] { return kHues[selTrack]; };
    }

    // ---- sequenceur ----
    for (int i = 0; i < 8; ++i)
        mkChip (slotChips[i], juce::String::charToString ((juce::juce_wchar)('A' + i)), [this, i]
        { if (copyArm) { proc.copySlotTo (i); copyArm = false; copyChip->setToggleState (false, juce::dontSendNotification); }
          else proc.gotoSlot (i);
          refreshChips(); repaint(); });
    mkChip (copyChip, "COPIER \u203a", [this] { copyArm = ! copyArm; copyChip->setToggleState (copyArm, juce::dontSendNotification); });
    mkChip (styChip, "PATTERN STYLE", [this] { proc.applyStyleSeqs (proc.styleIdx.load()); repaint(); });
    mkChip (rndChip, "AL\u00c9ATOIRE", [this] { proc.randomPattern(); repaint(); });
    mkChip (clrChip, "EFFACER", [this] { proc.clearPattern(); repaint(); });
    mkChip (songOnChip, "MODE SONG", [this]
    { const bool on = ! proc.songOn.load(); proc.songOn.store (on ? 1 : 0);
      songOnChip->setToggleState (on, juce::dontSendNotification); });
    mkChip (songAddChip, "+ PATTERN", [this]
    { const int n = proc.songLen.load();
      if (n < BzzzProcessor::kMaxSong)
      { proc.songSlot[n].store (proc.curSlot.load()); proc.songReps[n].store (1);
        proc.songLen.store (n + 1); rebuildSongUI(); } });
    mkChip (songClrChip, "VIDER", [this] { proc.songLen.store (0); proc.songPos.store (0); rebuildSongUI(); });

    // ---- synth ----
    mkChip (synPwrChip, "SYNTH ON", [this]
    { const bool on = ! proc.synthPower.load(); proc.synthPower.store (on ? 1 : 0);
      synPwrChip->setToggleState (on, juce::dontSendNotification); });
    synPwrChip->setToggleState (true, juce::dontSendNotification);

    auto SY = [this]() -> bzzz::SynCfg& { return proc.synCfg; };
    auto syKnob = [this, hueFn, SY] (ui::Knob& k, const juce::String& lab, float mn, float mx, bool lg,
                                     std::function<juce::String(float)> f, std::function<float(const bzzz::SynCfg&)> gv,
                                     std::function<void(bzzz::SynCfg&,float)> sv)
    {
        k.label = lab; k.mn = mn; k.mx = mx; k.logScale = lg; k.fmt = std::move (f); k.hue = hueFn;
        k.getV = [SY, gv] { return gv (SY()); };
        k.setV = [this, SY, sv] (float v) { const juce::ScopedLock l (proc.cfgLock); sv (SY(), v); };
        addAndMakeVisible (k);
    };
    // rangee OSC (identique au web)
    syKnob (sMix, "Mix", 0, 1, false, fmtPc, [](auto& s){return s.mix;}, [](auto& s,float v){s.mix=v;});
    syKnob (sO2, "OSC2\u00b1st", -24, 24, false, [](float v){return (v>0?"+":"")+juce::String((int)std::round(v));}, [](auto& s){return (float)s.osc2Pitch;}, [](auto& s,float v){s.osc2Pitch=(int)std::round(v);});
    syKnob (sDet, "Detune", 0, 40, false, [](float v){return juce::String((int)v)+"ct";}, [](auto& s){return s.detune;}, [](auto& s,float v){s.detune=v;});
    syKnob (sUni, "Unison", 1, 7, false, [](float v){return juce::String((int)std::round(v))+"v";}, [](auto& s){return (float)s.unison;}, [](auto& s,float v){s.unison=(int)std::round(v);});
    syKnob (sFM, "FM", 0, 1, false, fmtPc, [](auto& s){return s.fm;}, [](auto& s,float v){s.fm=v;});
    syKnob (sSub, "Sub", 0, 1, false, fmtPc, [](auto& s){return s.sub;}, [](auto& s,float v){s.sub=v;});
    syKnob (sNz, "Noise", 0, 1, false, fmtPc, [](auto& s){return s.noise;}, [](auto& s,float v){s.noise=v;});
    syKnob (sGl, "Glide", 0, .3f, false, fmtMs, [](auto& s){return s.glide;}, [](auto& s,float v){s.glide=v;});
    // rangee FILTRE : cutoff/res/env/lfoRate/lfoAmt via parametres (automatisables), le reste interne
    paramKnob (sCut, "scut", "Cutoff", fmtHz); sCut.logScale = true;
    paramKnob (sRes, "sres", "R\u00e9so", [](float v){return juce::String(v,1);});
    paramKnob (sEnv, "senv", "Env", fmtPc);
    syKnob (sFDec, "Decay", .03f, 1, false, fmtMs, [](auto& s){return s.fDec;}, [](auto& s,float v){s.fDec=v;});
    syKnob (sDrv, "Drive", 0, 1, false, fmtPc, [](auto& s){return s.drive;}, [](auto& s,float v){s.drive=v;});
    syKnob (sCr, "Crush", 0, 1, false, fmtPc, [](auto& s){return s.crush;}, [](auto& s,float v){s.crush=v;});
    // rangee ENV + LFO
    syKnob (sA, "Attack", .001f, 1, true, fmtMs, [](auto& s){return s.aA;}, [](auto& s,float v){s.aA=v;});
    syKnob (sD, "Decay", .02f, 1.5f, false, fmtMs, [](auto& s){return s.aD;}, [](auto& s,float v){s.aD=v;});
    syKnob (sS, "Sustain", 0, 1, false, fmtPc, [](auto& s){return s.aS;}, [](auto& s,float v){s.aS=v;});
    syKnob (sR, "Release", .02f, 2, false, fmtMs, [](auto& s){return s.aR;}, [](auto& s,float v){s.aR=v;});
    paramKnob (sLfoR, "slfor", "LFO Hz", [](float v){return juce::String(v,1);});
    paramKnob (sLfoA, "slfoa", "LFO\u2192Filt", fmtPc);
    syKnob (sLfoP, "LFO\u2192Pitch", 0, 1, false, fmtPc, [](auto& s){return s.lfoPitch;}, [](auto& s,float v){s.lfoPitch=v;});
    // rangee OUT
    syKnob (sW, "Width", 0, 1, false, fmtPc, [](auto& s){return s.width;}, [](auto& s,float v){s.width=v;});
    paramKnob (sVol, "svol", "Volume", fmtPc);
    syKnob (sSD, "\u2192Delay", 0, 1, false, fmtPc, [](auto& s){return s.sendD;}, [](auto& s,float v){s.sendD=v;});
    syKnob (sSR, "\u2192Space", 0, 1, false, fmtPc, [](auto& s){return s.sendR;}, [](auto& s,float v){s.sendR=v;});

    static const char* wf[4] = { "SAW","SQR","SIN","TRI" };
    for (int i = 0; i < 4; ++i)
    {
        mkChip (osc1C[i], wf[i], [this, i] { const juce::ScopedLock l (proc.cfgLock); proc.synCfg.osc1 = i; refreshChips(); }, true);
        mkChip (osc2C[i], wf[i], [this, i] { const juce::ScopedLock l (proc.cfgLock); proc.synCfg.osc2 = i; refreshChips(); }, true);
    }
    static const char* fmn[6] = { "LP24","LP12","HP","BP","NOTCH","FORMANT" };
    for (int i = 0; i < 6; ++i)
        mkChip (modeC[i], fmn[i], [this, i] { const juce::ScopedLock l (proc.cfgLock); proc.synCfg.fmode = i; refreshChips(); }, true);
    static const char* lfn[3] = { "SIN","SQR","SAW" };
    for (int i = 0; i < 3; ++i)
        mkChip (lfoC[i], lfn[i], [this, i] { const juce::ScopedLock l (proc.cfgLock); proc.synCfg.lfoShape = i; refreshChips(); }, true);
    mkChip (octDn, "OCT \u2212", [this] { proc.synOct.store (juce::jmax (0, proc.synOct.load() - 1)); repaint(); }, true);
    mkChip (octUp, "OCT +", [this] { proc.synOct.store (juce::jmin (4, proc.synOct.load() + 1)); repaint(); }, true);

    // ---- navigateurs ----
    auto styleBox = [] (juce::ComboBox& b)
    {
        b.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x14ffffff));
        b.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x22ffffff));
        b.setColour (juce::ComboBox::textColourId, juce::Colour (0xffF4F3F8));
        b.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff9A97AC));
    };
    for (int i = 0; i < kNumKits; ++i)   kitBox.addItem (juce::String (kitName (i)),   i + 1);
    for (int i = 0; i < kNumPatches; ++i) patBox.addItem (juce::String (patchName (i)), i + 1);
    styleBox (kitBox); styleBox (patBox);
    kitBox.onChange = [this] { const int i = kitBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.curKit.load()) { proc.loadKit (i); refreshInspector(); refreshChips(); repaint(); } };
    patBox.onChange = [this] { const int i = patBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.curPatch.load()) { proc.loadPatch (i); refreshChips(); repaint(); } };
    addAndMakeVisible (kitBox); addAndMakeVisible (patBox);
    kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification);
    patBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification);

    auto navK = [this] (int d) { proc.loadKit (proc.curKit.load() + d); kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification); refreshInspector(); refreshChips(); repaint(); };
    auto navP = [this] (int d) { proc.loadPatch (proc.curPatch.load() + d); patBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification); refreshChips(); repaint(); };
    mkChip (kitPrev, "\u2039", [navK] { navK (-1); }, true);
    mkChip (kitNext, "\u203a", [navK] { navK (+1); }, true);
    mkChip (kitRnd, "?", [this] { proc.loadKit (juce::Random::getSystemRandom().nextInt (kNumKits)); kitBox.setSelectedId (proc.curKit.load() + 1, juce::dontSendNotification); refreshInspector(); refreshChips(); repaint(); }, true);
    mkChip (patPrev, "\u2039", [navP] { navP (-1); }, true);
    mkChip (patNext, "\u203a", [navP] { navP (+1); }, true);
    mkChip (patRnd, "?", [this] { proc.loadPatch (juce::Random::getSystemRandom().nextInt (kNumPatches)); patBox.setSelectedId (proc.curPatch.load() + 1, juce::dontSendNotification); refreshChips(); repaint(); }, true);

    selectTrack (0);
    refreshChips();
    rebuildSongUI();
    startTimerHz (30);
}

void BzzzPage::refreshChips()
{
    for (int s = 0; s < 6; ++s) styleChips[s]->setToggleState (proc.styleIdx.load() == s, juce::dontSendNotification);
    for (int t = 0; t < 3; ++t) themeChips[t]->setToggleState (theme == t, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i) slotChips[i]->setToggleState (proc.curSlot.load() == i, juce::dontSendNotification);
    songOnChip->setToggleState (proc.songOn.load() != 0, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
    { osc1C[i]->setToggleState (proc.synCfg.osc1 == i, juce::dontSendNotification);
      osc2C[i]->setToggleState (proc.synCfg.osc2 == i, juce::dontSendNotification); }
    for (int i = 0; i < 6; ++i) modeC[i]->setToggleState (proc.synCfg.fmode == i, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) lfoC[i]->setToggleState (proc.synCfg.lfoShape == i, juce::dontSendNotification);
    synPwrChip->setToggleState (proc.synthPower.load() != 0, juce::dontSendNotification);
    repaint();
}
void BzzzPage::selectTrack (int t)
{
    selTrack = juce::jlimit (0, 11, t);
    for (int i = 0; i < 12; ++i) pads[i]->setToggleState (i == selTrack, juce::dontSendNotification);
    refreshInspector();
}
void BzzzPage::refreshInspector()
{
    const auto& c = proc.chCfg[selTrack];
    for (int i = 0; i < 6; ++i)
        fTypes[i]->setToggleState ((i == 0 && ! c.fon) || (i > 0 && c.fon && c.ftype == i - 1), juce::dontSendNotification);
    for (auto* k : { &ckCut,&ckRes,&ckDrv,&ckCr,&ckDe,&ckPi,&ckPa,&ckSD,&ckSR,&ckVo }) k->repaint();
    for (int i = 0; i < 12; ++i)
        padMutes[i]->setToggleState (proc.trackMute[i].load() != 0, juce::dontSendNotification);
    repaint();
}
void BzzzPage::rebuildSongUI()
{
    songEntries.clear(); songDels.clear();
    for (int i = 0; i < proc.songLen.load(); ++i)
    {
        auto* c = songEntries.add (new ui::Chip (juce::String::charToString ((juce::juce_wchar)('A' + proc.songSlot[i].load())) + " x" + juce::String (proc.songReps[i].load())));
        c->hue = [this] { return hue(); }; c->small = true;
        c->onClick = [this, i]
        { static const int reps[4] = { 1, 2, 4, 8 };
          int cur = proc.songReps[i].load(), idx = 0;
          for (int j = 0; j < 4; ++j) if (reps[j] == cur) idx = j;
          proc.songReps[i].store (reps[(idx + 1) % 4]); rebuildSongUI(); };
        addAndMakeVisible (*c);
        auto* d = songDels.add (new ui::Chip ("x"));
        d->hue = [] { return 355.f; }; d->small = true;
        d->onClick = [this, i]
        { const int n = proc.songLen.load();
          for (int j = i; j < n - 1; ++j)
          { proc.songSlot[j].store (proc.songSlot[j+1].load()); proc.songReps[j].store (proc.songReps[j+1].load()); }
          proc.songLen.store (n - 1);
          if (proc.songPos.load() >= juce::jmax (0, n - 1)) proc.songPos.store (0);
          rebuildSongUI(); };
        addAndMakeVisible (*d);
    }
    resized(); repaint();
}
void BzzzPage::timerCallback()
{
    const int s = proc.playStep.load();
    if (s != uiStep) { uiStep = s; repaint (gridArea); repaint (rollArea); }
    // les macros peuvent bouger par automation
    static int slow = 0;
    if (++slow % 6 == 0)
        for (auto* k : { &mCut,&mRes,&mSC,&mDrv,&mDly,&mRev,&mVol,&mSwing,&sCut,&sRes,&sEnv,&sLfoR,&sLfoA,&sVol })
            k->repaint();
}

// ------------------- geometrie -------------------
static juce::Rectangle<int> gridCell (juce::Rectangle<int> area, int t, int s)
{
    const int lab = 76;
    const int cw = (area.getWidth() - lab) / 16, ch = area.getHeight() / 12;
    return { area.getX() + lab + s * cw + 2, area.getY() + t * ch + 2, cw - 4, ch - 4 };
}
static juce::Rectangle<int> rollCellR (juce::Rectangle<int> area, int row, int s)
{
    const int lab = 76;
    const int cw = (area.getWidth() - lab) / 16, ch = area.getHeight() / 13;
    return { area.getX() + lab + s * cw + 2, area.getY() + (12 - row) * ch + 1, cw - 4, ch - 2 };
}

void BzzzPage::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (gridArea.contains (pt))
    {
        const int lab = 76;
        const int cw = (gridArea.getWidth() - lab) / 16, ch = gridArea.getHeight() / 12;
        const int s = (pt.x - gridArea.getX() - lab) / cw, t = (pt.y - gridArea.getY()) / ch;
        if (s >= 0 && s < 16 && t >= 0 && t < 12)
        { auto& c = proc.grid[t][s]; c.store ((c.load() + 1) % 3); repaint (gridArea); }
        return;
    }
    if (rollArea.contains (pt))
    {
        const int lab = 76;
        const int cw = (rollArea.getWidth() - lab) / 16, ch = rollArea.getHeight() / 13;
        const int s = (pt.x - rollArea.getX() - lab) / cw, rw = (pt.y - rollArea.getY()) / ch;
        if (s >= 0 && s < 16 && rw >= 0 && rw < 13)
        { const int note = 12 - rw;
          proc.rollNote[s].store (proc.rollNote[s].load() == note ? -100 : note);
          repaint (rollArea); }
        return;
    }
    if (accRow.contains (pt))
    { const int lab = 76; const int s = (pt.x - accRow.getX() - lab) / ((accRow.getWidth() - lab) / 16);
      if (s >= 0 && s < 16) { proc.rollAcc[s].store (proc.rollAcc[s].load() ? 0 : 1); repaint (accRow); } return; }
    if (sldRow.contains (pt))
    { const int lab = 76; const int s = (pt.x - sldRow.getX() - lab) / ((sldRow.getWidth() - lab) / 16);
      if (s >= 0 && s < 16) { proc.rollSlide[s].store (proc.rollSlide[s].load() ? 0 : 1); repaint (sldRow); } return; }
    if (kbdArea.contains (pt)) { kbdEvent (e, true); return; }
}
void BzzzPage::kbdEvent (const juce::MouseEvent& e, bool)
{
    // 25 demi-tons, 15 blanches ; noires prioritaires
    const auto pt = e.getPosition();
    const float x = (float)(pt.x - kbdArea.getX()) / kbdArea.getWidth();
    const float y = (float)(pt.y - kbdArea.getY()) / kbdArea.getHeight();
    static const int BLACK[12] = { 0,1,0,1,0,0,1,0,1,0,1,0 };
    int found = -1;
    if (y < .6f)
    {
        int whites = 0;
        for (int i = 0; i < 25; ++i)
        {
            if (BLACK[i % 12]) { const float bx = whites / 15.f - .021f;
                if (x >= bx && x < bx + .042f) { found = i; break; } }
            else ++whites;
        }
    }
    if (found < 0)
    {
        const int w = juce::jlimit (0, 14, (int)(x * 15));
        int whites = 0;
        for (int i = 0; i < 25; ++i) { if (! BLACK[i % 12]) { if (whites == w) { found = i; break; } ++whites; } }
    }
    if (found >= 0)
    {
        const int midiN = 12 * (proc.synOct.load() + 1) + found;
        if (kbdHeld >= 0 && kbdHeld != midiN) proc.queueUiNote (kbdHeld, false);
        if (kbdHeld != midiN) { proc.queueUiNote (midiN, true); kbdHeld = midiN; repaint (kbdArea); }
    }
}
void BzzzPage::mouseDrag (const juce::MouseEvent& e)
{ if (kbdHeld >= 0 && kbdArea.contains (e.getPosition())) kbdEvent (e, true); }
void BzzzPage::mouseUp (const juce::MouseEvent&)
{ if (kbdHeld >= 0) { proc.queueUiNote (kbdHeld, false); kbdHeld = -1; repaint (kbdArea); } }

// ------------------- paint -------------------
void BzzzPage::panel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    const float hu = hue();
    auto rf = r.toFloat();
    g.setColour (juce::Colour (0x09ffffff));
    g.fillRoundedRectangle (rf, 20.f);
    g.setColour (juce::Colour (0x17ffffff));
    g.drawRoundedRectangle (rf, 20.f, 1.f);
    // ligne haute accentuee
    juce::ColourGradient lg (juce::Colours::transparentBlack, rf.getX(), 0,
                             juce::Colours::transparentBlack, rf.getRight(), 0, false);
    lg.addColour (.5, ui::hsl (hu, 1.f, .6f, .5f));
    g.setGradientFill (lg);
    g.fillRect (rf.getX() + 20, rf.getY(), rf.getWidth() - 40, 1.5f);
    if (title.isNotEmpty())
    {
        g.setColour (ui::hsl (hu, 1.f, .58f));
        g.fillEllipse ((float) r.getX() + 18, (float) r.getY() + 15, 7, 7);
        g.setColour (juce::Colour (0xff9A97AC));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.f, juce::Font::bold)));
        g.drawText (title, r.getX() + 32, r.getY() + 9, 500, 18, juce::Justification::left);
    }
}

void BzzzPage::paint (juce::Graphics& g)
{
    const float hu = hue();
    const auto& th = ui::kThemes[theme];
    // fond nebuleuse comme le web
    g.fillAll (juce::Colour (th.bg0));
    {
        juce::ColourGradient v (juce::Colour (th.bg1), 0, 0, juce::Colour (th.bg0), 0, (float) H, false);
        g.setGradientFill (v); g.fillAll();
        juce::ColourGradient n1 (ui::hsl (hu, .9f, .5f, .16f), W * .18f, 300, ui::hsl (hu, .9f, .5f, 0), W * .18f + 620, 900, true);
        g.setGradientFill (n1); g.fillRect (0, 0, W, 1500);
        juce::ColourGradient n2 (ui::hsl (hu + 60, .9f, .55f, .13f), W * .85f, 1700, ui::hsl (hu + 60, .9f, .55f, 0), W * .85f - 700, 900, true);
        g.setGradientFill (n2); g.fillRect (0, 800, W, H - 800);
    }

    // header logo
    g.setFont (juce::Font (juce::FontOptions (27.f, juce::Font::bold)));
    g.setColour (juce::Colour (0xffF4F3F8));
    g.drawText ("B'ZZZ", 24, 16, 92, 30, juce::Justification::left);
    g.setColour (ui::hsl (hu, 1.f, .58f));
    g.drawText ("FREQUENCY", 112, 16, 190, 30, juce::Justification::left);
    g.setColour (juce::Colour (0xff5B5870));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain)));
    g.drawText ("PAYE TA RUCHE \u00b7 TECHNO REACTOR \u00b7 SYNC H\u00d4TE " + juce::String (proc.hostBpm.load(), 1) + " BPM",
                24, 46, 560, 12, juce::Justification::left);

    // panneaux
    panel (g, { 20, 76, 560, 470 }, "R\u00c9ACTEUR");
    panel (g, { 594, 76, W - 614, 470 }, "MACROS MASTER");
    panel (g, { 20, 560, W - 40, 128 }, "PADS \u2014 12 VOIX");
    panel (g, { 20, 702, W - 40, 168 }, "INSPECTEUR \u2014 " + juce::String (kNames[selTrack]));
    panel (g, { 20, 884, W - 40, 560 }, "S\u00c9QUENCEUR \u2014 PATTERNS A\u2013H \u00b7 SONG");
    panel (g, { 20, 1458, W - 40, 600 }, "FABLE ENGINE \u2014 SYNTH\u00c9TISEUR");
    panel (g, { 20, 2072, (W - 54) / 2, 66 }, "KITS \u2014 1000");
    panel (g, { 20 + (W - 54) / 2 + 14, 2072, (W - 54) / 2, 66 }, "PATCHES \u2014 1000");
    g.setColour (juce::Colour (0xff5B5870));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain)));
    g.drawText ("MIDI : canal 10 = drums (GM) \u00b7 autres canaux = synth\u00e9 \u00b7 MIDI map via l'h\u00f4te \u00b7 \u00e9tat sauvegard\u00e9 dans le projet",
                20, 2152, W - 40, 14, juce::Justification::centred);

    // ---- grille sequenceur ----
    for (int t = 0; t < 12; ++t)
    {
        auto lr = gridCell (gridArea, t, 0);
        g.setColour (ui::hsl (kHues[t], .9f, .62f));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::bold)));
        g.drawText (kNames[t], gridArea.getX(), lr.getY(), 70, lr.getHeight(), juce::Justification::centredRight);
        for (int s = 0; s < 16; ++s)
        {
            auto r = gridCell (gridArea, t, s).toFloat();
            const int v = proc.grid[t][s].load();
            juce::Colour c = (s % 4 == 0) ? juce::Colour (0x14ffffff) : juce::Colour (0x0affffff);
            if (v == 1) c = ui::hsl (kHues[t], .85f, .40f);
            if (v == 2) c = ui::hsl (kHues[t], .95f, .62f);
            if (s == uiStep) c = c.brighter (v > 0 ? .5f : .18f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 4.f);
            if (v == 2) { g.setColour (ui::hsl (kHues[t], 1.f, .62f, .5f)); g.drawRoundedRectangle (r.expanded (1.5f), 5.f, 1.5f); }
        }
    }

    // ---- clavier ----
    {
        auto ka = kbdArea.toFloat();
        static const int BLACK[12] = { 0,1,0,1,0,0,1,0,1,0,1,0 };
        const float ww = ka.getWidth() / 15.f;
        int whites = 0;
        const int held = kbdHeld - 12 * (proc.synOct.load() + 1);
        for (int i = 0; i < 25; ++i)
            if (! BLACK[i % 12])
            {
                juce::Rectangle<float> r (ka.getX() + whites * ww + 1, ka.getY(), ww - 2, ka.getHeight());
                g.setColour (i == held ? ui::hsl (hu, .9f, .6f) : juce::Colour (0xffE9E7F0));
                g.fillRoundedRectangle (r, 3.f);
                ++whites;
            }
        whites = 0;
        for (int i = 0; i < 25; ++i)
        {
            if (BLACK[i % 12])
            {
                juce::Rectangle<float> r (ka.getX() + whites * ww - ka.getWidth() * .021f, ka.getY(),
                                          ka.getWidth() * .042f, ka.getHeight() * .6f);
                g.setColour (i == held ? ui::hsl (hu, .9f, .5f) : juce::Colour (0xff17151E));
                g.fillRoundedRectangle (r, 3.f);
            }
            else ++whites;
        }
        g.setColour (juce::Colour (0xff9A97AC));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.f, juce::Font::plain)));
        g.drawText ("C" + juce::String (proc.synOct.load()), kbdArea.getX() - 60, kbdArea.getY() + kbdArea.getHeight()/2 - 7, 50, 14, juce::Justification::centredRight);
    }

    // ---- piano roll 13 rangees ----
    for (int rw2 = 0; rw2 < 13; ++rw2)
    {
        const int note = 12 - rw2;
        auto lr = rollCellR (rollArea, note, 0);
        const bool blackK = juce::String (kNoteNm[note % 12]).contains ("#");
        g.setColour (blackK ? juce::Colour (0xff5B5870) : juce::Colour (0xff9A97AC));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::plain)));
        g.drawText (juce::String (kNoteNm[note % 12]) + juce::String (proc.synOct.load() + note / 12),
                    rollArea.getX(), lr.getY(), 70, lr.getHeight(), juce::Justification::centredRight);
        for (int s = 0; s < 16; ++s)
        {
            auto r = rollCellR (rollArea, note, s).toFloat();
            const bool on = proc.rollNote[s].load() == note;
            juce::Colour c = (s % 4 == 0) ? juce::Colour (0x12ffffff) : juce::Colour (0x08ffffff);
            if (blackK && ! on) c = c.withMultipliedAlpha (.55f);
            if (on) c = ui::hsl (hu, .9f, proc.rollAcc[s].load() ? .68f : .52f);
            if (s == uiStep) c = c.brighter (on ? .4f : .15f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 3.f);
        }
    }
    // ACC / SLIDE
    auto rowPaint = [&] (juce::Rectangle<int> row, const juce::String& lab, std::atomic<int>* arr, float rowHue)
    {
        const int labW = 76;
        const int cw = (row.getWidth() - labW) / 16;
        g.setColour (juce::Colour (0xff9A97AC));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 8.5f, juce::Font::bold)));
        g.drawText (lab, row.getX(), row.getY(), 70, row.getHeight(), juce::Justification::centredRight);
        for (int s = 0; s < 16; ++s)
        {
            juce::Rectangle<float> r ((float) row.getX() + labW + s * cw + 2, (float) row.getY() + 2, (float) cw - 4, (float) row.getHeight() - 4);
            g.setColour (arr[s].load() ? ui::hsl (rowHue, .95f, .6f) : juce::Colour (0x0affffff));
            g.fillRoundedRectangle (r, 3.f);
        }
    };
    rowPaint (accRow, "ACC", proc.rollAcc, 48);
    rowPaint (sldRow, "SLIDE", proc.rollSlide, 200);
}

void BzzzPage::resized()
{
    auto place = [] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); };

    // header chips
    int x = 300;
    for (int s = 0; s < 6; ++s) { place (*styleChips[s], x, 18, 104, 28); x += 110; }
    x += 14;
    for (int t = 0; t < 3; ++t) { place (*themeChips[t], x, 18, 92, 28); x += 98; }

    // reacteur + macros
    reactor.setBounds (40, 104, 520, 424);
    { int kx = 620, ky = 122; ui::Knob* mk[8] = { &mCut,&mRes,&mSC,&mDrv,&mDly,&mRev,&mVol,&mSwing };
      const int kw = (W - 614 - 52) / 4;
      for (int i = 0; i < 8; ++i)
          place (*mk[i], kx + (i % 4) * kw, ky + (i / 4) * 204, kw - 8, 190); }

    // pads
    { const int pw = (W - 80) / 12;
      for (int i = 0; i < 12; ++i)
      { place (*pads[i], 30 + i * pw, 596, pw - 34, 66);
        place (*padMutes[i], 30 + i * pw + pw - 32, 596, 26, 26); } }

    // inspecteur
    { int fx = 36;
      for (int i = 0; i < 6; ++i) { place (*fTypes[i], fx, 734, 66, 26); fx += 72; }
      ui::Knob* ik[10] = { &ckCut,&ckRes,&ckDrv,&ckCr,&ckDe,&ckPi,&ckPa,&ckSD,&ckSR,&ckVo };
      const int kw = (W - 72) / 10;
      for (int i = 0; i < 10; ++i) place (*ik[i], 36 + i * kw, 764, kw - 8, 96); }

    // sequenceur
    { int sx = 36;
      for (int i = 0; i < 8; ++i) { place (*slotChips[i], sx, 916, 44, 28); sx += 50; }
      sx += 10; place (*copyChip, sx, 916, 96, 28); sx += 104;
      place (*styChip, sx, 916, 140, 28); sx += 148;
      place (*rndChip, sx, 916, 110, 28); sx += 118;
      place (*clrChip, sx, 916, 96, 28);
      gridArea = { 36, 956, W - 72, 384 };
      int gy = 1352;
      place (*songOnChip, 36, gy, 116, 28);
      place (*songAddChip, 160, gy, 106, 28);
      place (*songClrChip, 274, gy, 76, 28);
      int ex = 366;
      for (int i = 0; i < songEntries.size(); ++i)
      { place (*songEntries[i], ex, gy, 62, 28); ex += 66;
        place (*songDels[i], ex, gy, 26, 28); ex += 32;
        if (ex > W - 120) break; } }

    // synth
    { int sy = 1490;
      place (*synPwrChip, 36, sy, 100, 28);
      int ox = 150;
      for (int i = 0; i < 4; ++i) { place (*osc1C[i], ox, sy, 54, 26); ox += 58; }
      ox += 12;
      for (int i = 0; i < 4; ++i) { place (*osc2C[i], ox, sy, 54, 26); ox += 58; }
      ox += 16;
      for (int i = 0; i < 6; ++i) { place (*modeC[i], ox, sy, 74, 26); ox += 78; }
      ox += 12;
      for (int i = 0; i < 3; ++i) { place (*lfoC[i], ox, sy, 52, 26); ox += 56; }

      // 4 rangees de knobs (8 / 6 / 7 / 4+oct)
      const int rowY = sy + 40, rH = 100;
      { ui::Knob* r1[8] = { &sMix,&sO2,&sDet,&sUni,&sFM,&sSub,&sNz,&sGl };
        const int kw = (W - 72) / 8;
        for (int i = 0; i < 8; ++i) place (*r1[i], 36 + i * kw, rowY, kw - 8, 96); }
      { ui::Knob* r2[6] = { &sCut,&sRes,&sEnv,&sFDec,&sDrv,&sCr };
        const int kw = (W - 72) / 6;
        for (int i = 0; i < 6; ++i) place (*r2[i], 36 + i * kw, rowY + rH, kw - 8, 96); }
      { ui::Knob* r3[7] = { &sA,&sD,&sS,&sR,&sLfoR,&sLfoA,&sLfoP };
        const int kw = (W - 72) / 7;
        for (int i = 0; i < 7; ++i) place (*r3[i], 36 + i * kw, rowY + rH * 2, kw - 8, 96); }
      { ui::Knob* r4[4] = { &sW,&sVol,&sSD,&sSR };
        const int kw = (W - 72) / 7;
        for (int i = 0; i < 4; ++i) place (*r4[i], 36 + i * kw, rowY + rH * 3, kw - 8, 96);
        place (*octDn, 36 + 5 * kw, rowY + rH * 3 + 30, 70, 28);
        place (*octUp, 36 + 5 * kw + 130, rowY + rH * 3 + 30, 70, 28); }

      kbdArea  = { 112, sy + 40 + rH * 4 + 6, W - 148, 64 };
      rollArea = { 36, kbdArea.getBottom() + 10, W - 72, 13 * 22 };
      accRow   = { 36, rollArea.getBottom() + 2, W - 72, 20 };
      sldRow   = { 36, accRow.getBottom() + 2, W - 72, 20 }; }

    // navigateurs
    { const int pw = (W - 54) / 2;
      place (*kitPrev, 36, 2100, 30, 28);
      kitBox.setBounds (72, 2100, pw - 140, 28);
      place (*kitNext, 72 + pw - 134, 2100, 30, 28);
      place (*kitRnd, 72 + pw - 100, 2100, 30, 28);
      const int px = 20 + pw + 14 + 16;
      place (*patPrev, px, 2100, 30, 28);
      patBox.setBounds (px + 36, 2100, pw - 140, 28);
      place (*patNext, px + 36 + pw - 134, 2100, 30, 28);
      place (*patRnd, px + 36 + pw - 100, 2100, 30, 28); }
}

// =====================================================================
//                             EDITOR
// =====================================================================
BzzzEditor::BzzzEditor (BzzzProcessor& p)
    : AudioProcessorEditor (p), proc (p), page (p, *this)
{
    holder.addAndMakeVisible (page);
    view.setViewedComponent (&holder, false);
    view.setScrollBarsShown (true, false);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (660, 460, 2800, 2000);
    setSize (juce::jlimit (660, 2800, proc.edW.load()), juce::jlimit (460, 2000, proc.edH.load()));
}

void BzzzEditor::resized()
{
    view.setBounds (getLocalBounds());
    const int sbw = view.getScrollBarThickness();
    const float scale = (float) (getWidth() - sbw) / (float) BzzzPage::W;
    page.setTransform (juce::AffineTransform::scale (scale));
    page.setTopLeftPosition (0, 0);
    holder.setSize (getWidth() - sbw, (int) std::ceil (BzzzPage::H * scale));
    proc.edW.store (getWidth());
    proc.edH.store (getHeight());
}
