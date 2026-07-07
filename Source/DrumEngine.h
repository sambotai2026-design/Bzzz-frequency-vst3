/*  B'Zzz Frequency — moteur 12 voix, portage C++ du moteur Web Audio.
    100% synthèse, zéro sample. */
#pragma once
#include "Presets.h"
#include <cmath>
#include <cstdint>
#include <array>

namespace bzzz
{
static constexpr float kPi = 3.14159265358979f;
static constexpr float kTau = 6.28318530717959f;

// ---------- utilitaires ----------
struct Rng            // xorshift léger pour le bruit
{
    uint32_t s = 0x9E3779B9u;
    inline float white()  { s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                            return (float)(int32_t)s * 4.6566129e-10f; }
};

struct Pink           // bruit rose (Paul Kellet), comme dans la version web
{
    float b0=0,b1=0,b2=0;
    Rng rng;
    inline float next()
    {
        const float w = rng.white();
        b0 = 0.99765f*b0 + w*0.0990460f;
        b1 = 0.96300f*b1 + w*0.2965164f;
        b2 = 0.57000f*b2 + w*1.0526913f;
        return (b0 + b1 + b2 + w*0.1848f) * 0.25f;
    }
};

struct SVF            // filtre state-variable TPT (lp/bp/hp)
{
    float g=0.5f, k=1.0f, ic1=0, ic2=0;
    void set (float cutoff, float res, float sr)
    {
        cutoff = cutoff < 20.f ? 20.f : (cutoff > sr*0.45f ? sr*0.45f : cutoff);
        g = std::tan (kPi * cutoff / sr);
        const float q = res < 0.1f ? 0.1f : res;
        k = 1.0f / q;
    }
    inline void step (float in, float& lp, float& bp, float& hp)
    {
        hp = (in - ic2 - (g + k) * ic1) / (1.0f + g * (g + k));
        bp = g * hp + ic1;  ic1 = g * hp + bp;
        lp = g * bp + ic2;  ic2 = g * bp + lp;
    }
    inline float lowpass  (float in) { float l,b,h; step(in,l,b,h); return l; }
    inline float bandpass (float in) { float l,b,h; step(in,l,b,h); return b; }
    inline float highpass (float in) { float l,b,h; step(in,l,b,h); return h; }
    void reset() { ic1 = ic2 = 0; }
};

inline float expDecay (float t, float tau) { return std::exp (-t / tau); }
inline float drive    (float x, float amt) { return std::tanh (x * (1.0f + amt * 6.0f)); }

// ---------- une voix générique ----------
/* Chaque voix rend son signal par échantillon à partir du temps écoulé
   depuis son déclenchement. decayMul et pitchSemi viennent des réglages piste. */
struct Voice
{
    bool  active = false;
    float t = 0;               // secondes depuis le trigger
    float vel = 1, sr = 48000;
    float decayMul = 1, pitchMul = 1;
    float ph1=0, ph2=0, ph3=0, sweepPh=0;
    Rng   rng;  Pink pink;  SVF f1, f2;
    int   type = 0;            // index de TID

    void trigger (int typeIdx, float velocity, float sampleRate,
                  float decay, float pitchSemi)
    {
        type = typeIdx; vel = velocity; sr = sampleRate;
        decayMul = decay; pitchMul = std::pow (2.0f, pitchSemi / 12.0f);
        t = 0; ph1 = ph2 = ph3 = sweepPh = 0;
        f1.reset(); f2.reset();
        active = true;
    }

    inline float sine (float& ph, float f)
    { ph += f / sr; if (ph >= 1) ph -= 1; return std::sin (ph * kTau); }
    inline float square (float& ph, float f)
    { ph += f / sr; if (ph >= 1) ph -= 1; return ph < 0.5f ? 1.f : -1.f; }
    inline float saw (float& ph, float f)
    { ph += f / sr; if (ph >= 1) ph -= 1; return ph * 2.f - 1.f; }

    // banc métallique 808 : 6 carrés à ratios inharmoniques (comme la version web)
    inline float metal (float base)
    {
        static constexpr float R[6] = {2.f,3.f,4.16f,5.43f,6.79f,8.21f};
        float s = 0; float ph = ph1;
        for (int i = 0; i < 6; ++i)
        {
            float p = std::fmod (ph * R[i], 1.0f);
            s += (p < 0.5f ? 1.f : -1.f);
        }
        ph1 += base / sr; if (ph1 >= 1) ph1 -= 1;
        return s / 6.0f;
    }

    float render()
    {
        if (! active) return 0;
        const float d = decayMul;
        float out = 0;

        switch (type)
        {
        case 0: { // KICK : drop sinus + sub + click
            const float f = (50.f + 110.f * expDecay (t, 0.03f)) * pitchMul;
            float body = sine (ph1, f) * expDecay (t, 0.28f * d);
            float sub  = sine (ph2, f * 0.5f) * expDecay (t, 0.35f * d) * 0.5f;
            float clk  = (t < 0.006f) ? f1.highpass (rng.white()) * expDecay (t, 0.004f) * 0.8f : 0.f;
            if (t < 0.001f) f1.set (3000.f, 0.7f, sr);
            out = (body + sub) * 1.1f + clk;
            if (t > 0.7f * d) active = false;
        } break;
        case 1: { // RUMBLE : nappe basse sombre
            const float f = 55.f * pitchMul;
            f1.set (300.f, 0.8f, sr);
            float n = f1.lowpass (pink.next()) * 1.6f;
            out = (sine (ph1, f) * 0.7f + n * 0.5f) * expDecay (t, 0.5f * d) * 0.8f;
            if (t > 1.4f * d) active = false;
        } break;
        case 2: { // SNARE : 2 tons + bruit rose BP + snap
            float tone = (sine (ph1, 185.f*pitchMul) + sine (ph2, 330.f*pitchMul)) * 0.5f
                         * expDecay (t, 0.08f * d);
            f1.set (1800.f, 1.2f, sr);
            float noise = f1.bandpass (pink.next()) * 2.2f * expDecay (t, 0.14f * d);
            float snap  = (t < 0.01f) ? rng.white() * expDecay (t, 0.006f) * 0.6f : 0.f;
            out = tone * 0.8f + noise + snap;
            if (t > 0.45f * d) active = false;
        } break;
        case 3: { // CLAP : 3 bursts + queue
            const float burst = (t < 0.011f || (t > 0.011f && t < 0.022f) || (t > 0.022f && t < 0.033f))
                                ? expDecay (std::fmod (t, 0.011f), 0.004f) : 0.f;
            const float tail  = (t > 0.033f) ? expDecay (t - 0.033f, 0.09f * d) : 0.f;
            f1.set (1200.f, 1.6f, sr);
            out = f1.bandpass (rng.white()) * (burst + tail) * 2.6f;
            if (t > 0.4f * d) active = false;
        } break;
        case 4: { // CHH : banc métallique -> HP
            f1.set (7200.f, 0.9f, sr);
            out = f1.highpass (metal (410.f * pitchMul)) * expDecay (t, 0.03f * d) * 1.1f;
            if (t > 0.14f * d) active = false;
        } break;
        case 5: { // OHH
            f1.set (6800.f, 0.9f, sr);
            out = f1.highpass (metal (400.f * pitchMul)) * expDecay (t, 0.22f * d) * 0.95f;
            if (t > 0.9f * d) active = false;
        } break;
        case 6: { // RIDE : métal + shimmer
            f1.set (8200.f, 0.8f, sr);
            float m = f1.highpass (metal (520.f * pitchMul));
            f2.set (9000.f, 0.7f, sr);
            float sh = f2.highpass (rng.white()) * 0.25f;
            out = (m + sh) * expDecay (t, 0.5f * d) * 0.55f;
            if (t > 1.8f * d) active = false;
        } break;
        case 7: { // PERC : ping résonant
            f1.set (760.f * pitchMul, 24.f, sr);
            float exc = (t < 0.003f) ? rng.white() : 0.f;
            out = f1.bandpass (exc) * 6.0f * expDecay (t, 0.11f * d);
            if (t > 0.5f * d) active = false;
        } break;
        case 8: { // TOM : drop 200->90
            const float f = (90.f + 110.f * expDecay (t, 0.05f)) * pitchMul;
            out = sine (ph1, f) * expDecay (t, 0.22f * d) * 1.0f;
            if (t > 0.6f * d) active = false;
        } break;
        case 9: { // ACID : saw + LP env (303-ish)
            const float f = 55.f * pitchMul;   // A1 * pitch piste
            const float cut = 300.f + 2800.f * expDecay (t, 0.09f * d);
            f1.set (cut, 8.0f, sr);
            out = f1.lowpass (saw (ph1, f) + saw (ph2, f * 1.005f)) * expDecay (t, 0.2f * d) * 0.9f;
            if (t > 0.5f * d) active = false;
        } break;
        case 10: { // STAB : accord mineur saw -> BP
            const float f = 110.f * pitchMul;
            float s = saw (ph1, f) + saw (ph2, f * 1.1892f) + saw (ph3, f * 1.4983f); // m3 + 5te
            f1.set (900.f, 2.2f, sr);
            out = f1.bandpass (s) * expDecay (t, 0.13f * d) * 0.9f;
            if (t > 0.5f * d) active = false;
        } break;
        case 11: { // RISE : sweep de bruit montant (2 temps env.)
            const float dur = 1.0f * d;
            const float pos = t / dur;
            if (pos >= 1) { active = false; break; }
            f1.set (300.f + 6000.f * pos * pos, 3.0f, sr);
            out = f1.bandpass (rng.white()) * (0.25f + pos * 0.75f) * 1.6f;
        } break;
        }

        t += 1.0f / sr;
        return out * vel;
    }
};

// ---------- moteur : 12 pistes, polyphonie 2 par piste, stéréo + sends ----------
struct Engine
{
    static constexpr int kTracks = 12;
    std::array<ChannelCfg, kTracks> ch;
    std::array<std::array<Voice, 2>, kTracks> voices; // round-robin x2
    std::array<int, kTracks> rr {};
    std::array<SVF, kTracks> chF;
    float sr = 48000;
    float kickPulse = 0;   // pour le sidechain

    void prepare (double sampleRate) { sr = (float) sampleRate; }

    void trigger (int track, float vel)
    {
        if (track < 0 || track >= kTracks) return;
        auto& v = voices[(size_t) track][(size_t) (rr[(size_t) track] ^= 1)];
        v.trigger (track, vel, sr, ch[(size_t) track].decay, ch[(size_t) track].pitch);
        if (track == 0) kickPulse = 1.0f;  // kick -> sidechain
    }

    // rend un échantillon stéréo + accumule les sends delay/reverb
    inline void render (float& L, float& R, float& sendDly, float& sendRev)
    {
        L = R = sendDly = sendRev = 0;
        for (int tI = 0; tI < kTracks; ++tI)
        {
            float s = 0;
            for (auto& v : voices[(size_t) tI]) if (v.active) s += v.render();
            if (s == 0.0f && ! voices[(size_t) tI][0].active && ! voices[(size_t) tI][1].active) continue;
            const auto& c = ch[(size_t) tI];
            if (c.drive > 0.001f) s = drive (s, c.drive);
            if (c.crush > 0.01f)
            {
                const float levels = 6.0f + (1.0f - c.crush) * (1.0f - c.crush) * 250.0f;
                s = std::round (s * levels) / levels;
            }
            if (c.fon)
            {
                auto& f = chF[(size_t) tI];
                f.set (c.cutoff, c.res, sr);
                switch (c.ftype)
                {
                    case 0: s = f.lowpass (s); break;
                    case 1: s = f.highpass (s); break;
                    case 2: s = f.bandpass (s) * 1.5f; break;
                    case 3: { float lp, bp, hp; f.step (s, lp, bp, hp); s = lp + hp; } break;
                    default:{ float lp, bp, hp; f.step (s, lp, bp, hp); s = s + bp * 0.8f; } break;
                }
            }
            s *= c.vol;
            const float pl = c.pan <= 0 ? 1.0f : 1.0f - c.pan;
            const float pr = c.pan >= 0 ? 1.0f : 1.0f + c.pan;
            L += s * pl; R += s * pr;
            sendDly += s * c.sendD;
            sendRev += s * c.sendR;
        }
        kickPulse *= 0.9993f;   // relâchement du sidechain (~120 ms)
    }
};
} // namespace bzzz
