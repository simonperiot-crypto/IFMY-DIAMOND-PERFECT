#pragma once
#include <complex>
#include <vector>
#include <cmath>

namespace dsp
{
    // FFT radix-2 itérative (C++ pur, mémoire allouée une seule fois dans prepare)
    class FFT
    {
    public:
        void prepare (int order)
        {
            n = 1 << order;
            tw.resize ((size_t) n / 2);
            for (int i = 0; i < n / 2; ++i)
                tw[(size_t) i] = std::polar (1.0f, (float) (-2.0 * 3.14159265358979323846 * i / n));
            rev.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                int r = 0;
                for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
                rev[(size_t) i] = r;
            }
            buf.resize ((size_t) n);
        }

        int size() const { return n; }

        // Spectre de magnitude d'un signal réel (n échantillons) -> n/2 valeurs
        void magnitude (const float* in, float* mag)
        {
            for (int i = 0; i < n; ++i) buf[(size_t) rev[(size_t) i]] = { in[i], 0.0f };
            for (int len = 2; len <= n; len <<= 1)
            {
                const int half = len / 2, step = n / len;
                for (int i = 0; i < n; i += len)
                    for (int j = 0; j < half; ++j)
                    {
                        const auto t = tw[(size_t) (j * step)] * buf[(size_t) (i + j + half)];
                        const auto u = buf[(size_t) (i + j)];
                        buf[(size_t) (i + j)] = u + t;
                        buf[(size_t) (i + j + half)] = u - t;
                    }
            }
            for (int i = 0; i < n / 2; ++i) mag[i] = std::abs (buf[(size_t) i]);
        }

    private:
        int n = 0;
        std::vector<std::complex<float>> tw, buf;
        std::vector<int> rev;
    };
}
