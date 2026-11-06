#pragma once
#include "DSP.h"

namespace dsp
{
    //==========================================================================
    struct Gate
    {
        void prepare (double s) { sr = s; env = 0; gain = 1; att = coefFromMs (1.0, sr); rel = coefFromMs (90.0, sr); envRel = coefFromMs (40.0, sr); }
        void setThreshold (float db) { thr = dbToGain (db); thrLow = dbToGain (db - 4.0f); }

        inline void process (float* f, int n)
        {
            float pk = 0;
            for (int c = 0; c < n; ++c) pk = std::max (pk, std::abs (f[c]));
            env = pk > env ? pk : env + (pk - env) * envRel;
            if (env > thr) open = true;
            else if (env < thrLow) open = false;
            const float tgt = open ? 1.0f : 0.0f;
            gain += (tgt - gain) * (tgt > gain ? att : rel);
            for (int c = 0; c < n; ++c) f[c] *= gain;
        }

        double sr = 48000;
        float env = 0, gain = 1, att = 1, rel = 1, envRel = 1, thr = 0.001f, thrLow = 0.0006f;
        bool open = true;
    };

    //==========================================================================
    struct DeEsser
    {
        void prepare (double s) { sr = s; lp[0] = lp[1] = 0; env = 0; att = coefFromMs (0.5, sr); rel = coefFromMs (60.0, sr); setFreq (6500); }
        void setFreq (float f) { a = (float) (1.0 - std::exp (-2.0 * kPiD * f / sr)); }
        void setThreshold (float db) { thrDb = db; }

        inline void process (float* f, int n)
        {
            float hf[2], lo[2], pk = 0;
            for (int c = 0; c < n; ++c)
            {
                lp[c] += a * (f[c] - lp[c]);
                lo[c] = lp[c];
                hf[c] = f[c] - lp[c];
                pk = std::max (pk, std::abs (hf[c]));
            }
            const float over = gainToDb (pk) - thrDb;
            const float gr = over > 0 ? over * 0.75f : 0.0f;
            env += (gr - env) * (gr > env ? att : rel);
            const float g = dbToGain (-env);
            for (int c = 0; c < n; ++c) f[c] = lo[c] + hf[c] * g;
            grDb = env;
        }

        double sr = 48000;
        float lp[2] { 0, 0 }, a = 0.5f, env = 0, att = 1, rel = 1, thrDb = -30, grDb = 0;
    };

    //==========================================================================
    struct Compressor
    {
        void prepare (double s) { sr = s; env = 0; set (-18, 3, 5, 120, 0); }

        void set (float thrDb, float ratio, float attackMs, float releaseMs, float makeupDb)
        {
            thr = thrDb; slope = 1.0f - 1.0f / std::max (1.0f, ratio);
            att = coefFromMs (attackMs, sr); rel = coefFromMs (releaseMs, sr); makeup = makeupDb;
        }

        inline void process (float* f, int n)
        {
            float pk = 0;
            for (int c = 0; c < n; ++c) pk = std::max (pk, std::abs (f[c]));
            const float x = gainToDb (pk) - thr;
            constexpr float knee = 6.0f;
            float gr;
            if (2 * x < -knee)                 gr = 0;
            else if (2 * std::abs (x) <= knee) gr = slope * (x + knee / 2) * (x + knee / 2) / (2 * knee);
            else                               gr = slope * x;
            env += (gr - env) * (gr > env ? att : rel);
            const float g = dbToGain (makeup - env);
            for (int c = 0; c < n; ++c) f[c] *= g;
            grDb = env;
        }

        double sr = 48000;
        float thr = -18, slope = 0.66f, att = 1, rel = 1, makeup = 0, env = 0, grDb = 0;
    };

    //==========================================================================
    struct Saturator
    {
        void set (float driveDb, float mixAmt)
        {
            drive = dbToGain (driveDb);
            comp = 1.0f / std::pow (drive, 0.62f);
            mix = mixAmt;
        }

        inline void process (float* f, int n)
        {
            for (int c = 0; c < n; ++c)
            {
                const float x = f[c];
                const float y = std::tanh (x * drive + 0.06f * drive * x * x) * comp; // légère asymétrie = harmoniques paires (chaleur)
                f[c] = x + mix * (y - x);
            }
        }

        float drive = 1, comp = 1, mix = 0;
    };

    //==========================================================================
    // Doubleur : deux voix décalées et légèrement désaccordées, ouvertes en stéréo
    struct Doubler
    {
        void prepare (double s)
        {
            sr = s;
            for (auto& d : dl) d.prepare ((int) (sr * 0.06));
            ph[0] = 0; ph[1] = 0.37f;
            inc[0] = (float) (0.31 / sr); inc[1] = (float) (0.23 / sr);
        }

        inline void process (float* f, int n, float mix, float width)
        {
            const float in = n > 1 ? 0.5f * (f[0] + f[1]) : f[0];
            float w[2];
            for (int s = 0; s < 2; ++s)
            {
                ph[s] += inc[s];
                if (ph[s] >= 1) ph[s] -= 1;
                const float base = (float) (sr * (s == 0 ? 0.0115 : 0.0172));
                const float depth = (float) (sr * 0.0014);
                dl[s].push (in);
                w[s] = dl[s].read (base + depth * std::sin (2 * kPi * ph[s]));
            }
            if (n > 1)
            {
                const float mid = 0.5f * (w[0] + w[1]);
                f[0] += mix * 0.8f * (mid + width * (w[0] - mid));
                f[1] += mix * 0.8f * (mid + width * (w[1] - mid));
            }
            else
            {
                f[0] += mix * 0.4f * (w[0] + w[1]);
            }
        }

        double sr = 48000;
        DelayLine dl[2];
        float ph[2] { 0, 0 }, inc[2] { 0, 0 };
    };

    //==========================================================================
    struct StereoDelay
    {
        void prepare (double s)
        {
            sr = s;
            for (auto& d : dl) d.prepare ((int) (sr * 4.2));
            lp[0] = lp[1] = 0;
            time = (float) (sr * 0.25);
            smooth = coefFromMs (60.0, sr);
        }

        inline void process (float* f, int n, float targetSamples, float fb, float toneCoef, float mix, bool ping)
        {
            time += (std::clamp (targetSamples, 4.0f, (float) (sr * 4.0)) - time) * smooth;
            const float yL = dl[0].read (time), yR = dl[1].read (time);
            lp[0] += toneCoef * (yL - lp[0]);
            lp[1] += toneCoef * (yR - lp[1]);

            const float inL = f[0], inR = n > 1 ? f[1] : f[0];
            if (ping)
            {
                dl[0].push (0.5f * (inL + inR) + lp[1] * fb);
                dl[1].push (lp[0] * fb);
            }
            else
            {
                dl[0].push (inL + lp[0] * fb);
                dl[1].push (inR + lp[1] * fb);
            }

            if (n > 1) { f[0] += mix * lp[0]; f[1] += mix * lp[1]; }
            else         f[0] += mix * 0.5f * (lp[0] + lp[1]);
        }

        double sr = 48000;
        DelayLine dl[2];
        float lp[2] { 0, 0 }, time = 1000, smooth = 0.001f;
    };
}
