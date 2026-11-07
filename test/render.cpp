// Test offline: syntetyczna gitara (Karplus-Strong) -> Stonoga @4x -> WAV.
#include "../Source/Presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <random>

static const int SR = 48000, OS = 4;

static void ks(std::vector<float>& out, int start, float freq, float dur, float amp, std::mt19937& rng, float damp = 0.996f) {
    int N = (int)(SR / freq); std::vector<float> buf((size_t)N);
    std::uniform_real_distribution<float> u(-1, 1);
    for (auto& b : buf) b = u(rng) * amp;
    int len = (int)(dur * SR); size_t p = 0;
    for (int i = 0; i < len && start + i < (int)out.size(); ++i) {
        float a = buf[p], b = buf[(p + 1) % N];
        buf[p] = (a + b) * 0.5f * damp;
        out[(size_t)(start + i)] += a;
        p = (p + 1) % N;
    }
}

static void writeWav(const std::string& fn, const std::vector<float>& d) {
    FILE* f = fopen(fn.c_str(), "wb"); int n = (int)d.size(), bytes = n * 2;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(1); w32(SR); w32(SR * 2); w16(2); w16(16);
    fwrite("data", 1, 4, f); w32(bytes);
    for (float s : d) { int v = (int)std::lround(stonoga::clampf(s, -1.f, 1.f) * 32767); w16((uint16_t)(int16_t)v); }
    fclose(f);
}

int main() {
    std::mt19937 rng(7);
    const float secs = 9.f;
    std::vector<float> gtr((size_t)(secs * SR), 0.f);
    // riff: power chordy E/G/A + pojedyncze nuty + wybrzmiewanie
    struct N { float t, f, d; int chord; };
    const N notes[] = {{0.0f, 82.4f, .45f, 1}, {0.5f, 82.4f, .45f, 1}, {1.0f, 98.0f, .45f, 1}, {1.5f, 110.f, .9f, 1},
                       {2.5f, 82.4f, .2f, 0}, {2.75f, 82.4f, .2f, 0}, {3.0f, 146.8f, .3f, 0}, {3.3f, 164.8f, .3f, 0}, {3.6f, 196.f, .6f, 0},
                       {4.4f, 110.f, 4.5f, 1}};
    for (auto& n : notes) {
        int st = (int)(n.t * SR);
        ks(gtr, st, n.f, n.d, 0.35f, rng);
        if (n.chord) { ks(gtr, st + 120, n.f * 1.4983f, n.d, 0.3f, rng); ks(gtr, st + 240, n.f * 2.f, n.d, 0.25f, rng); }
    }
    writeWav("clean.wav", gtr);

    for (int pi = 0; pi < stonoga::kNumPresets; ++pi) {
        stonoga::Engine e; e.prepare(SR * OS, 1);
        const auto& pr = stonoga::kPresets[pi];
        std::vector<float> up(gtr.size() * OS);
        for (size_t i = 0; i < gtr.size(); ++i) {
            float a = gtr[i], b = i + 1 < gtr.size() ? gtr[i + 1] : 0.f;
            for (int k = 0; k < OS; ++k) up[i * OS + k] = a + (b - a) * k / (float)OS;
        }
        const int blk = 512 * OS;
        for (size_t o = 0; o < up.size(); o += blk) {
            float* ptr = up.data() + o; int n = (int)std::min<size_t>(blk, up.size() - o);
            e.process(&ptr, 1, n, pr.p);
        }
        // decymacja z prostym filtrem (średnia z 4 + lekkie wygładzenie)
        std::vector<float> out(gtr.size()); float lp = 0;
        double sum2 = 0; float peak = 0; int nans = 0; double dc = 0;
        for (size_t i = 0; i < out.size(); ++i) {
            float s = 0; for (int k = 0; k < OS; ++k) s += up[i * OS + k]; s /= OS;
            lp += (s - lp) * 0.7f; out[i] = lp;
            if (!std::isfinite(lp)) nans++;
            sum2 += lp * lp; peak = std::max(peak, std::fabs(lp)); dc += lp;
        }
        // RMS w ciszy po riffie? ostatnie 0.3 s
        double tail = 0; for (size_t i = out.size() - SR * 3 / 10; i < out.size(); ++i) tail += out[i] * out[i];
        printf("%-20s rms=%.3f peak=%.3f dc=%.4f nan=%d tailRms=%.4f\n", pr.name, std::sqrt(sum2 / out.size()), peak, dc / out.size(), nans, std::sqrt(tail / (SR * 0.3)));
        std::string fn = "p" + std::to_string(pi) + ".wav";
        writeWav(fn, out);
    }
}
