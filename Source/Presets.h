/* B'Zzz Frequency V2 — générateur de presets.
   Port fidèle de la version web : mêmes seeds mulberry32, même ordre d'appels
   => les 1000 kits et 1000 patches sont les mêmes que dans le plugin HTML. */
#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

namespace bzzz
{
static constexpr int kTracks = 12, kSteps = 16;
static constexpr int kNumKits = 1000, kNumPatches = 1000;

// ---- PRNG mulberry32 (identique au JS) ----
struct Mulberry
{
    uint32_t a;
    explicit Mulberry (uint32_t seed) : a (seed) {}
    double operator()()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a;
        t = (uint32_t)((uint64_t)(t ^ (t >> 15)) * (uint64_t)(1u | t));
        t = (uint32_t)(t + (uint32_t)((uint64_t)(t ^ (t >> 7)) * (uint64_t)(61u | t))) ^ t;
        return double ((t ^ (t >> 14))) / 4294967296.0;
    }
};

inline double fix (double v, int dp)  // équivalent de +x.toFixed(dp)
{
    const double m = std::pow (10.0, dp);
    return std::round (v * m) / m;
}

// ---- noms ----
static const char* kTID[kTracks] = { "kick","rumble","snare","clap","chh","ohh","ride","perc","tom","acid","stab","rise" };

static const char* kKitFams[40] = {
 "Warehouse","Bunker","Concrete","Steel","Modular","Raw Hardware","Acid Basement","Detroit Soul",
 "Motor City","UK Bleep","Birmingham","Peak Time","Mainstage","Hard Storm","Gabber Core","Industrial",
 "EBM Body","Hypno Loop","Dub Chamber","Deep Field","Minimal Lab","Tribal Rites","Afro Circuit","Latin Voltage",
 "French Riviera","Disco Machine","Italo Night","Garage 2step","Break Engine","Glitch Unit","IDM Fractal",
 "Psy Forest","Goa Sunrise","Ghetto Bass","Electro Funk","Rave 92","Trance Gate","Melodic Dawn","Progressive","Coldwave" };

static const char* kPatchFams[40] = {
 "Rave Hoover","Hypersaw","Mega Saw","Trance Lead","Sync Lead","PWM Lead","Hard Lead","Detuned Lead",
 "Dark Pluck","Bell Pluck","Pluck House","Pluck Arp","Glass Keys","Soft Keys","Organ Bass","Rolling Bass",
 "Deep Sub","Dark Sub","Sub 808","Reese","Hard Reese","FM Growl","Wobble","Bit Bass","Donk","Acid 303",
 "Acid Dark","Mentanol","Warehouse Stab","Brass Stab","Stab Chord","Rave Stab","Formant Vox","Talk Box",
 "Choir","Air Pad","Dream Pad","Glass Pad","Detroit Strings","Resonant Drone" };

// styles : 0 berlin 1 detroit 2 london 3 paris 4 rotterdam 5 tbilisi
static const char* kStyleNames[6] = { "berlin","detroit","london","paris","rotterdam","tbilisi" };

inline int kitStyleOf (const std::string& fam)
{
    struct M { const char* f; int s; };
    static const M map[] = {
      {"Warehouse",0},{"Bunker",0},{"Concrete",0},{"Steel",0},{"Modular",0},{"Raw Hardware",0},
      {"Acid Basement",2},{"Detroit Soul",1},{"Motor City",1},{"UK Bleep",2},{"Birmingham",2},
      {"Peak Time",2},{"Mainstage",2},{"Hard Storm",4},{"Gabber Core",4},{"Industrial",4},
      {"EBM Body",4},{"Hypno Loop",5},{"Dub Chamber",5},{"Deep Field",5},{"Minimal Lab",5},
      {"Tribal Rites",5},{"Afro Circuit",3},{"Latin Voltage",3},{"French Riviera",3},{"Disco Machine",3},
      {"Italo Night",3},{"Garage 2step",2},{"Break Engine",2},{"Glitch Unit",1},{"IDM Fractal",1},
      {"Psy Forest",5},{"Goa Sunrise",5},{"Ghetto Bass",2},{"Electro Funk",1},{"Rave 92",2},
      {"Trance Gate",0},{"Melodic Dawn",3},{"Progressive",3},{"Coldwave",0} };
    for (auto& m : map) if (fam == m.f) return m.s;
    return 0;
}

inline bool has (const std::string& fam, std::initializer_list<const char*> subs)
{
    for (auto* s : subs) if (fam.find (s) != std::string::npos) return true;
    return false;
}

// ---- structures ----
struct ChannelCfg
{
    bool  fon = false;
    int   ftype = 0;       // 0 lowpass 1 highpass 2 bandpass 3 notch 4 peaking
    float cutoff = 12000, res = 0.8f, drive = 0, crush = 0,
          decay = 1, pitch = 0, pan = 0, sendD = 0, sendR = 0, vol = 0.8f;
};

struct Kit
{
    std::string name;
    int style = 0;
    int grid[kTracks][kSteps] {};
    ChannelCfg ch[kTracks];
};

struct SynCfg
{
    int   osc1 = 0, osc2 = 1;   // 0 saw 1 square 2 sine 3 triangle
    float mix = .3f;
    float detune = 10; int unison = 3; int osc2Pitch = 0;
    float fm = 0, sub = .4f, noise = .03f, glide = .06f;
    int   fmode = 0;            // 0 lp24 1 lp12 2 hp 3 bp 4 notch 5 formant
    float cutoff = 600, res = 6, envAmt = .75f, fDec = .16f;
    float drive = .3f, crush = 0;
    float lfoRate = 5, lfoAmt = 0, lfoPitch = 0; int lfoShape = 0; // 0 sine 1 square 2 saw
    float aA = .002f, aD = .18f, aS = .1f, aR = .2f;
    float width = .5f, vol = .6f, sendD = .25f, sendR = .25f;
};

struct Patch
{
    std::string name;
    SynCfg syn;
    int  note[kSteps];   // -100 = null
    int  acc[kSteps] {}, slide[kSteps] {};
};

inline std::string two (int n) { return (n < 10 ? "0" : "") + std::to_string (n); }
inline std::string kitName   (int i) { return std::string (kKitFams[i / 25])   + " " + two (i % 25 + 1); }
inline std::string patchName (int i) { return std::string (kPatchFams[i / 25]) + " " + two (i % 25 + 1); }

inline ChannelCfg defCH (float vol)
{
    ChannelCfg c; c.vol = vol; return c;
}

// ---- genKit : ordre d'appels r() identique au JS ----
inline Kit genKit (int i)
{
    Mulberry r ((uint32_t)((int64_t) i * 2654435761LL + 7));
    auto rr = [&] (double a, double b) { return a + (b - a) * r(); };
    auto ri = [&] (int a, int b) { return (int) std::floor (rr (a, b + 1)); };
    const std::string fam = kKitFams[i / 25];

    Kit k; k.name = kitName (i); k.style = kitStyleOf (fam);
    auto& g = k.grid;
    enum { KICK, RUMBLE, SNARE, CLAP, CHH, OHH, RIDE, PERC, TOM, ACID, STAB, RISE };

    if (has (fam, {"Gabber","Hard","Storm","Steel","Industrial"}))
        { for (int j = 0; j < 16; j += 2) g[KICK][j] = (j % 4 == 0) ? 2 : 1; }
    else if (has (fam, {"Break","Garage","Glitch","IDM","Electro"}))
        { g[KICK][0] = 2; g[KICK][10] = 1; if (r() < .6) g[KICK][6] = 1; g[KICK][ri (2, 4)] = 1; }
    else
        { for (int j = 0; j < 16; j += 4) g[KICK][j] = (j == 0) ? 2 : 1;
          if (has (fam, {"Rave","Peak","Main"}) && r() < .5) g[KICK][14] = 1; }

    const int ho = has (fam, {"Minimal","Deep","Dub","Hypno"}) ? 2 : 1;
    const double chp = has (fam, {"Tribal","Afro","Latin","Rave"}) ? .8 : .58;
    for (int j = 0; j < 16; ++j) if (j % ho == 0 && r() < chp) g[CHH][j] = 1;

    for (int j = 2; j < 16; j += 4) if (r() < .65) g[OHH][j] = 1;

    for (int j : {4, 12})
        if (r() < .8) { int* row = (r() < .5) ? g[CLAP] : g[SNARE]; row[j] = (r() < .4) ? 2 : 1; }

    if (has (fam, {"Garage","Break"}))
        { g[SNARE][4] = 0; g[CLAP][4] = 0; g[SNARE][6] = 1; g[SNARE][ri (10, 11)] = 1; }

    const double pd = has (fam, {"Tribal","Afro","Latin"}) ? .3 : .13;
    for (int j = 0; j < 16; ++j) if (r() < pd) g[PERC][j] = 1;

    if (r() < .4) g[TOM][ri (8, 15)] = 1;

    if (has (fam, {"Peak","Rave","Detroit","Motor"}))
        for (int j = 8; j < 16; j += 4) if (r() < .5) g[RIDE][j] = 1;

    if (has (fam, {"Warehouse","Bunker","Dub","Deep","Hypno","Trance"}))
        for (int j = 2; j < 16; j += 4) g[RUMBLE][j] = 1;

    if (has (fam, {"Acid","Bleep","Birmingham","Psy","Goa"}))
        for (int j = 0; j < 16; ++j) if (r() < .4) g[ACID][j] = (r() < .2) ? 2 : 1;

    if (has (fam, {"French","Disco","Italo","Melodic","Progressive","Riviera"}))
        for (int j = 0; j < 16; j += 3) if (r() < .6) g[STAB][j] = 1;

    if (r() < .25) g[RISE][0] = 1;
    if (g[KICK][0] == 0) g[KICK][0] = 2;

    static const int ftypes[5] = { 0, 1, 2, 3, 4 }; // lowpass highpass bandpass notch peaking
    for (int tI = 0; tI < kTracks; ++tI)
    {
        const std::string t = kTID[tI];
        ChannelCfg c = defCH (.8f);
        if (t == "kick") c.vol = .95f; if (t == "rumble") c.vol = .8f;
        if (t == "ride") c.vol = .4f;  if (t == "rise")   c.vol = .5f;

        if (has (fam, {"Gabber","Hard","Industrial","Steel","Raw","EBM"}))
        { c.drive = (float) fix (rr (.25, .65), 2); if (t == "kick") c.decay = (float) fix (rr (.6, 1), 2); }

        if (has (fam, {"Glitch","IDM","Ghetto","Bit"}) && r() < .45)
            c.crush = (float) fix (rr (.15, .5), 2);

        if (has (fam, {"Dub","Deep","Hypno","Minimal"}))
        { if (t == "kick") c.decay = (float) fix (rr (1.2, 2), 2);
          if (t == "chh" || t == "perc") c.sendD = (float) fix (rr (.2, .5), 2);
          c.sendR = (float) fix (rr (.1, .4), 2); }

        if (has (fam, {"Tribal","Afro","Latin"}) && (t == "perc" || t == "tom"))
        { c.pan = (float) fix (rr (-.7, .7), 2); c.pitch = (float) fix (rr (-4, 4), 1); }

        if ((t == "perc" || t == "clap" || t == "stab" || t == "ohh") && r() < .4)
            c.pan = (float) fix (rr (-.5, .5), 2);

        if (r() < .28)
        { c.fon = true; c.ftype = ftypes[(int) std::floor (r() * 5)];
          c.cutoff = (float) std::round (rr (300, 9000)); c.res = (float) fix (rr (.5, 6), 1); }

        if (t == "kick") c.pitch = (float) fix (rr (-2, 2), 1);
        k.ch[tI] = c;
    }
    return k;
}

// ---- genPatch : ordre identique au JS ----
inline Patch genPatch (int i)
{
    Mulberry r ((uint32_t)((int64_t) i * 40503LL + 13));
    auto rr = [&] (double a, double b) { return a + (b - a) * r(); };
    const std::string fam = kPatchFams[i / 25];
    auto pkI = [&] (std::initializer_list<int> a)
    { int idx = (int) std::floor (r() * (double) a.size()); return *(a.begin() + idx); };

    const bool bass = has (fam, {"Bass","Sub","Reese","Wobble","Organ","Donk","Growl"});
    const bool lead = has (fam, {"Lead","Saw","Hoover","Sync","Stab"});
    const bool pad  = has (fam, {"Pad","Drone","Choir","Strings","Glass","Air"});
    const bool acid = has (fam, {"Acid","Mentanol"});

    Patch p; p.name = patchName (i);
    SynCfg& s = p.syn;
    // 0 saw 1 square 2 sine 3 triangle — ordres identiques aux tableaux JS
    s.osc1 = pkI ({0,0,1,3});
    s.osc2 = pkI ({0,1,2,3});
    s.mix  = (float) fix (rr (.1, .6), 2);
    s.detune = (float) fix (lead ? rr (14,36) : (bass ? rr (0,7) : rr (2,18)), 0);
    s.unison = lead ? pkI ({5,7,7}) : (bass ? 1 : pkI ({1,3,4,5}));
    s.osc2Pitch = pkI ({0,0,7,12,-12,5,-7});
    s.fm = has (fam, {"FM","Growl","Bell","Donk","Bit"}) ? (float) fix (rr (.2,.7), 2)
         : (r() < .2 ? (float) fix (rr (0,.25), 2) : 0.0f);
    s.sub   = (float) fix (bass ? rr (.5,.95) : rr (0,.4), 2);
    s.noise = (float) fix (has (fam,{"Air","Sweep","Drone"}) ? rr (.08,.3) : rr (0,.07), 2);
    s.glide = (float) fix (acid ? rr (.04,.1) : rr (0,.06), 3);
    s.fmode = has (fam,{"Vox","Talk","Choir"}) ? 5 : (bass ? 0 : pkI ({0,0,1,2,3,4}));
    s.cutoff = (float) std::round (bass ? rr (160,750) : (pad ? rr (700,3500) : rr (300,6000)));
    s.res = (float) fix (acid ? rr (11,18) : (has (fam,{"Resonant","Bell","Donk"}) ? rr (8,15) : rr (1,8)), 1);
    s.envAmt = (float) fix (acid ? rr (.7,.95) : rr (.2,.85), 2);
    s.fDec  = (float) fix (rr (.07,.55), 2);
    s.drive = (float) fix (has (fam,{"Hard","Bit","Warehouse","Rave"}) ? rr (.3,.6) : rr (0,.35), 2);
    s.crush = has (fam,{"Bit"}) ? (float) fix (rr (.2,.5), 2)
            : (r() < .12 ? (float) fix (rr (0,.25), 2) : 0.0f);
    s.lfoRate = (float) fix (has (fam,{"Wobble"}) ? rr (2,7) : rr (.3,8), 1);
    s.lfoAmt  = has (fam,{"Wobble"}) ? (float) fix (rr (.4,.8), 2)
              : (pad ? (float) fix (rr (.1,.4), 2) : (r() < .3 ? (float) fix (rr (0,.2), 2) : 0.0f));
    s.lfoPitch = (r() < .15) ? (float) fix (rr (0,.3), 2) : 0.0f;
    s.lfoShape = has (fam,{"Wobble"}) ? pkI ({0,1}) : pkI ({0,0,1,2});
    s.aA = (float) fix (pad ? rr (.05,.45) : rr (.001,.02), 3);
    s.aD = (float) fix (rr (.1,.55), 2);
    s.aS = (float) fix (pad ? rr (.6,.9) : (bass ? rr (.1,.4) : rr (.05,.5)), 2);
    s.aR = (float) fix (pad ? rr (.4,1.5) : rr (.05,.4), 2);
    s.width = (float) fix ((lead || pad) ? rr (.6,1) : rr (.1,.6), 2);
    s.vol   = (float) fix (rr (.5,.68), 2);
    s.sendD = (float) fix (rr (0,.4), 2);
    s.sendR = (float) fix (pad ? rr (.3,.6) : rr (0,.4), 2);

    static const int sc[7] = {0,3,5,7,10,12,15};
    const double dens = rr (.35,.7);
    bool any = false;
    for (int j = 0; j < 16; ++j)
    {
        p.note[j] = -100;
        if (r() < dens)
        {
            p.note[j] = sc[(int) std::floor (r() * 7)];
            any = true;
            if (r() < .25) p.acc[j] = 1;
            if (r() < .2)  p.slide[j] = 1;
        }
    }
    if (! any) p.note[0] = 0;
    return p;
}

// ---- styles : tempo/swing/root ----
struct StyleInfo { float bpm; float swing; int root; };
static const StyleInfo kStyles[6] = {
    {132, 6, 45}, {128, 4, 55}, {140, 0, 55}, {122, 14, 55}, {150, 0, 50}, {126, 8, 45} };

// ---- patterns de style (stylePattern du web) ----
inline void stylePattern (int st, int g[kTracks][kSteps])
{
    for (int tI = 0; tI < kTracks; ++tI) for (int s = 0; s < kSteps; ++s) g[tI][s] = 0;
    enum { KICK, RUMBLE, SNARE, CLAP, CHH, OHH, RIDE, PERC, TOM, ACID, STAB, RISE };
    auto four = [&] (int tr, bool a) { for (int i = 0; i < 16; i += 4) g[tr][i] = (i == 0 && a) ? 2 : 1; };
    switch (st)
    {
    case 0: four (KICK,true); for (int i:{2,6,10,14}) g[RUMBLE][i]=1;
        for (int i:{0,2,4,6,8,10,12,14}) g[CHH][i]=1; g[CHH][15]=1;
        for (int i:{2,6,10,14}) g[OHH][i]=1; g[CLAP][4]=1; g[CLAP][12]=1;
        for (int i:{3,9,14}) g[PERC][i]=1; g[TOM][11]=1; break;
    case 1: four (KICK,true); for (int i:{4,12}) g[SNARE][i]=2;
        for (int i=0;i<16;++i) g[CHH][i]=1; for (int i:{2,6,10,14}) g[OHH][i]=1;
        g[RIDE][8]=1; for (int i:{0,3,6,8,11,14}) g[STAB][i]=(i%8==0)?2:1; break;
    case 2: four (KICK,true); g[KICK][14]=1;
        for (int i:{4,12}) g[CLAP][i]=2; for (int i=0;i<16;++i) g[CHH][i]=1;
        for (int i:{2,6,10,14}) g[OHH][i]=1;
        for (int i:{0,2,3,5,7,8,10,12,13,15}) g[ACID][i]=(i%8==0)?2:1; g[RISE][0]=1; break;
    case 3: four (KICK,true); for (int i:{4,12}) g[CLAP][i]=1; g[SNARE][15]=1;
        for (int i:{1,3,5,7,9,11,13,15}) g[CHH][i]=1; for (int i:{2,6,10,14}) g[OHH][i]=1;
        for (int i:{0,6,10}) g[STAB][i]=(i==0)?2:1; for (int i:{3,9,12,14}) g[RUMBLE][i]=1; break;
    case 4: for (int i=0;i<16;i+=2) g[KICK][i]=(i%4==0)?2:1;
        for (int i:{4,12}) g[SNARE][i]=1; for (int i=0;i<16;++i) if (i%2) g[CHH][i]=1;
        for (int i:{7,15}) g[TOM][i]=1; g[RISE][8]=1; break;
    case 5: four (KICK,true); for (int i:{2,6,10,14}) { g[RUMBLE][i]=1; g[OHH][i]=1; }
        for (int i:{0,4,8,12}) g[CHH][i]=1; for (int i:{5,13}) g[PERC][i]=1; break;
    }
}

// ---- mélodies de style (STYLE_ROLL) — N = null ----
struct StyleRoll { int note[16]; int acc[16]; int slide[16]; };
static constexpr int N_ = -100;
static const StyleRoll kStyleRolls[6] = {
 {{N_,N_,0,N_,N_,N_,0,N_,N_,N_,0,N_,N_,N_,0,3},{0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
 {{0,N_,N_,7,N_,N_,3,N_,0,N_,N_,10,N_,N_,7,N_},{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},{0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0}},
 {{0,N_,12,3,N_,7,N_,15,0,N_,12,N_,3,15,N_,10},{1,0,0,0,0,0,0,1,1,0,0,0,0,1,0,0},{0,0,1,0,0,0,0,1,0,0,1,0,0,1,0,0}},
 {{0,N_,N_,7,N_,5,N_,N_,3,N_,7,N_,N_,10,N_,12},{1,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
 {{0,N_,0,N_,N_,N_,12,N_,0,N_,0,N_,N_,N_,15,N_},{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
 {{N_,0,N_,N_,0,N_,N_,3,N_,0,N_,N_,-2,N_,N_,0},{0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0},{0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}} };

} // namespace bzzz
