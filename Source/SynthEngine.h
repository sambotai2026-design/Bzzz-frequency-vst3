/* B'Zzz Frequency V2 — FABLE ENGINE.
   Port du synthé web : 2 OSC + sub + noise, unison 7 voix, FM, glide,
   filtre lp24/lp12/hp/bp/notch/formant, ADSR, LFO (filtre + pitch),
   drive, crush, largeur stéréo. */
#pragma once
#include "DrumEngine.h"   // SVF, Rng, drive()
#include "Presets.h"

namespace bzzz
{
struct SynthVoice
{
    static constexpr int kMaxUni = 7;
    bool  active = false, released = false;
    float sr = 48000;
    float noteHz = 110, targetHz = 110, glideCoef = 0;
    float velo = 1;
    double t = 0, relT = 0;
    float ph1[kMaxUni] {}, ph2[kMaxUni] {}, subPh = 0, lfoPh = 0;
    float envLevel = 0;      // suivi pour le release
    Rng   rng;
    SVF   fA, fB, fFor1, fFor2;
    const SynCfg* cfg = nullptr;

    void start (float hz, float vel, float sampleRate, const SynCfg& c, bool slideFrom, float fromHz)
    {
        cfg = &c; sr = sampleRate; velo = vel;
        targetHz = hz;
        if (slideFrom && active) { noteHz = fromHz; }           // slide 303 : on glisse
        else if (! active)       { noteHz = hz; for (auto& p : ph1) p = rng.white()*0.5f+0.5f; }
        if (! slideFrom) noteHz = active ? noteHz : hz;
        if (! active || ! slideFrom) { t = 0; relT = 0; released = false; }
        else                          { released = false; relT = 0; }
        glideCoef = (c.glide > 0.0005f) ? std::exp (-1.0f / (c.glide * sr)) : 0.0f;
        active = true;
    }
    void release() { if (active && ! released) { released = true; relT = 0; } }

    static inline float oscShape (int type, float ph)
    {
        switch (type)
        {
            case 0: return ph * 2.0f - 1.0f;                             // saw
            case 1: return ph < 0.5f ? 1.0f : -1.0f;                     // square
            case 2: return std::sin (ph * kTau);                         // sine
            default: return ph < 0.5f ? (ph*4.0f-1.0f) : (3.0f-ph*4.0f); // triangle
        }
    }

    // rend un échantillon ; pan par voix d'unison géré par l'appelant via index
    void render (float& outL, float& outR)
    {
        if (! active || cfg == nullptr) { outL = outR = 0; return; }
        const SynCfg& c = *cfg;

        // glide
        noteHz = targetHz + (noteHz - targetHz) * glideCoef;

        // LFO
        lfoPh += c.lfoRate / sr; if (lfoPh >= 1) lfoPh -= 1;
        float lfo = c.lfoShape == 0 ? std::sin (lfoPh * kTau)
                  : c.lfoShape == 1 ? (lfoPh < 0.5f ? 1.f : -1.f)
                  : (lfoPh * 2.f - 1.f);

        const float pitchMod = std::pow (2.0f, (lfo * c.lfoPitch) / 2.0f);
        const float f1 = noteHz * pitchMod;
        const float f2 = f1 * std::pow (2.0f, c.osc2Pitch / 12.0f);

        // FM : osc2 module la phase d'osc1
        float sL = 0, sR = 0;
        const int uni = c.unison < 1 ? 1 : (c.unison > kMaxUni ? kMaxUni : c.unison);
        for (int u = 0; u < uni; ++u)
        {
            const float spread = uni > 1 ? ((float) u / (uni - 1) * 2.0f - 1.0f) : 0.0f;
            const float det = std::pow (2.0f, spread * c.detune / 1200.0f);
            ph2[u] += f2 * det / sr; if (ph2[u] >= 1) ph2[u] -= 1;
            const float o2 = oscShape (c.osc2, ph2[u]);
            float phm = ph1[u] + o2 * c.fm * 0.25f;
            phm -= std::floor (phm);
            ph1[u] += f1 * det / sr; if (ph1[u] >= 1) ph1[u] -= 1;
            const float o1 = oscShape (c.osc1, phm);
            const float v = o1 * (1.0f - c.mix) + o2 * c.mix;
            const float panv = spread * c.width;
            sL += v * (1.0f - panv * 0.5f);
            sR += v * (1.0f + panv * 0.5f);
        }
        sL /= (float) uni; sR /= (float) uni;

        // sub + noise (mono au centre)
        subPh += (f1 * 0.5f) / sr; if (subPh >= 1) subPh -= 1;
        const float subv = std::sin (subPh * kTau) * c.sub;
        const float nz = rng.white() * c.noise;
        sL += subv + nz; sR += subv + nz;

        // enveloppe ampli ADSR
        float env;
        if (! released)
        {
            if (t < c.aA) env = (float)(t / c.aA);
            else { const double dt = t - c.aA;
                   env = c.aS + (1.0f - c.aS) * std::exp (-(float) dt / (c.aD > 0.01f ? c.aD : 0.01f)); }
            envLevel = env;
        }
        else
        {
            env = envLevel * std::exp (-(float) relT / (c.aR > 0.01f ? c.aR : 0.01f));
            relT += 1.0 / sr;
            if (env < 0.0008f) { active = false; outL = outR = 0; return; }
        }

        // enveloppe filtre (decay) + LFO
        const float fenv = std::exp (-(float) t / (c.fDec > 0.02f ? c.fDec : 0.02f));
        float cut = c.cutoff * (1.0f + c.envAmt * 5.0f * fenv) * (1.0f + lfo * c.lfoAmt * 1.5f);
        cut = cut < 30.f ? 30.f : (cut > sr * 0.45f ? sr * 0.45f : cut);

        float mono = (sL + sR) * 0.5f;
        float fl = 0;
        switch (c.fmode)
        {
            case 0: { fA.set (cut, c.res, sr); float a = fA.lowpass (mono);
                      fB.set (cut, 0.9f, sr);  fl = fB.lowpass (a); } break;   // lp24
            case 1: fA.set (cut, c.res, sr); fl = fA.lowpass (mono); break;    // lp12
            case 2: fA.set (cut, c.res, sr); fl = fA.highpass (mono); break;   // hp
            case 3: fA.set (cut, c.res, sr); fl = fA.bandpass (mono) * 1.6f; break; // bp
            case 4: { fA.set (cut, c.res, sr); float lp, bp, hp; fA.step (mono, lp, bp, hp);
                      fl = lp + hp; } break;                                    // notch
            default: { fFor1.set (cut * 0.9f + 250.f, 7.f, sr);
                       fFor2.set (cut * 1.9f + 700.f, 8.f, sr);
                       fl = (fFor1.bandpass (mono) + fFor2.bandpass (mono)) * 1.8f; } break; // formant
        }

        // drive + crush
        if (c.drive > 0.001f) fl = drive (fl, c.drive);
        if (c.crush > 0.01f)
        {
            const float levels = 6.0f + (1.0f - c.crush) * (1.0f - c.crush) * 250.0f;
            fl = std::round (fl * levels) / levels;
        }

        // re-stéréoïse en gardant la largeur relative
        const float side = (sR - sL) * 0.5f * 0.35f;
        outL = (fl - side) * env * velo;
        outR = (fl + side) * env * velo;

        t += 1.0 / sr;
    }
};

// ---- moteur synthé : 1 voix séquenceur (avec slides) + pool poly MIDI ----
struct SynthEngine
{
    static constexpr int kPoly = 6;
    SynCfg cfg;
    SynthVoice seqVoice;                 // mono, slides 303
    std::array<SynthVoice, kPoly> poly;  // clavier MIDI
    std::array<int, kPoly> polyNote { -1,-1,-1,-1,-1,-1 };
    int rrIdx = 0;
    float sr = 48000;
    float lastSeqHz = 110;

    void prepare (double sampleRate) { sr = (float) sampleRate; }

    void seqNote (float hz, bool accent, bool slideFlag)
    {
        seqVoice.start (hz, accent ? 1.0f : 0.62f, sr, cfg, slideFlag, lastSeqHz);
        lastSeqHz = hz;
    }
    void seqRelease() { seqVoice.release(); }

    void noteOn (int midiNote, float vel)
    {
        const float hz = 440.0f * std::pow (2.0f, (midiNote - 69) / 12.0f);
        auto& v = poly[(size_t) rrIdx];
        polyNote[(size_t) rrIdx] = midiNote;
        rrIdx = (rrIdx + 1) % kPoly;
        v.start (hz, vel, sr, cfg, false, hz);
    }
    void noteOff (int midiNote)
    {
        for (int i = 0; i < kPoly; ++i)
            if (polyNote[(size_t) i] == midiNote) { poly[(size_t) i].release(); polyNote[(size_t) i] = -1; }
    }
    void allOff() { seqVoice.release(); for (auto& v : poly) v.release(); }

    void render (float& L, float& R)
    {
        float l = 0, r = 0, tl, tr;
        seqVoice.render (tl, tr); l += tl; r += tr;
        for (auto& v : poly) if (v.active) { v.render (tl, tr); l += tl; r += tr; }
        L = l * cfg.vol; R = r * cfg.vol;
    }
};
} // namespace bzzz
