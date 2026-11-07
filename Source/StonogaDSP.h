// Stonoga FX — rdzeń przetwarzania (bez zależności od JUCE).
// Przester do noise rocka: kaskadowy fuzz, zagłodzona bateria, rozpadająca się bramka,
// oktawa, nosowa "maska", rdza (bitcrush + decymacja), trzaski i sprzężenie zwrotne.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace stonoga {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float v, float lo, float hi) { return std::min(hi, std::max(lo, v)); }

struct Params {
    float drive    = 0.65f; // przester 0..1  (6..72 dB)
    float starve   = 0.0f;  // głód baterii: niższe szyny, asymetria, charczenie
    float gate     = 0.0f;  // bramka-rozpad: velcro/sputter przy wygasaniu
    float octave   = 0.0f;  // oktawa w górę (prostowanie)
    float mask     = 0.0f;  // nosowy filtr samogłosek
    float vowel    = 0.5f;  // u-o-a-e-i
    float rust     = 0.0f;  // bitcrush + decymacja
    float crackle  = 0.0f;  // trzaski zależne od sygnału
    float feedback = 0.0f;  // sprzężenie (może samo-oscylować)
    float fbTime   = 0.3f;  // czas sprzężenia 1..80 ms
    float tone     = 0.5f;  // barwa: ciemno <-> jasno
    float mix      = 1.0f;  // dry/wet
    float outDb    = -10.f; // wyjście
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
    void set(float nb0, float nb1, float nb2, float a0, float na1, float na2) {
        b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0;
    }
    static void wcs(float sr, float f, float q, float& c, float& al) {
        float w = 2 * kPi * clampf(f, 10.f, sr * 0.45f) / sr;
        c = std::cos(w); al = std::sin(w) / (2 * q);
    }
    void lowpass(float sr, float f, float q)  { float c, al; wcs(sr, f, q, c, al); set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass(float sr, float f, float q) { float c, al; wcs(sr, f, q, c, al); set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void bandpass(float sr, float f, float q) { float c, al; wcs(sr, f, q, c, al); set(al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void peak(float sr, float f, float q, float db) {
        float c, al; wcs(sr, f, q, c, al); float A = std::pow(10.f, db / 40.f);
        set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf(float sr, float f, float db) {
        float c, al; wcs(sr, f, 0.7071f, c, al); float A = std::pow(10.f, db / 40.f), s = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
            (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf(float sr, float f, float db) {
        float c, al; wcs(sr, f, 0.7071f, c, al); float A = std::pow(10.f, db / 40.f), s = 2 * std::sqrt(A) * al;
        set(A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
            (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
};

class Engine {
public:
    void prepare(double sampleRate, int numChannels) {
        sr = (float)sampleRate;
        ch.assign((size_t)std::max(1, numChannels), Chan{});
        uint32_t seed = 0x9E3779B9u;
        for (auto& c : ch) {
            c.fb.assign((size_t)(sr * 0.1f) + 8, 0.f);
            c.rng = (seed += 0x7F4A7C15u);
        }
        smooth = Params{};
        first = true;
    }

    // io: kanały (już nadpróbkowane), przetwarzanie w miejscu.
    void process(float* const* io, int numCh, int n, const Params& target) {
        if (ch.empty() || n <= 0) return;
        // wygładzanie parametrów raz na blok (bloki nadpróbkowane są krótkie)
        auto sm = [&](float& a, float b) { a = first ? b : a + (b - a) * 0.35f; };
        sm(smooth.drive, target.drive);   sm(smooth.starve, target.starve); sm(smooth.gate, target.gate);
        sm(smooth.octave, target.octave); sm(smooth.mask, target.mask);     sm(smooth.vowel, target.vowel);
        sm(smooth.rust, target.rust);     sm(smooth.crackle, target.crackle); sm(smooth.feedback, target.feedback);
        sm(smooth.fbTime, target.fbTime); sm(smooth.tone, target.tone);     sm(smooth.mix, target.mix);
        sm(smooth.outDb, target.outDb);
        first = false;
        const Params& p = smooth;

        // --- wartości na blok ---
        const float totalDb = 6.f + p.drive * 66.f;
        const float g = std::pow(10.f, totalDb / 20.f);
        const float g1 = std::pow(g, 0.5f), g2 = std::pow(g, 0.3f), g3 = std::pow(g, 0.2f);
        const float rail = 1.f - 0.82f * p.starve;
        const float bias = p.starve * 0.5f;
        const float makeup = 1.f / (rail * (1.f + 0.6f * p.starve));
        const float fold = clampf((p.drive - 0.7f) / 0.3f, 0.f, 1.f);
        const float gateThr = std::max(p.gate * 0.06f, p.starve * p.starve * 0.025f);
        const bool gateOn = gateThr > 1e-5f;
        const int fbDelay = std::max(1, (int)(sr * (0.001f + p.fbTime * 0.079f)));
        const float fbAmt = p.feedback * 1.25f;
        const float bits = 12.f - p.rust * 10.5f;
        const float levels = std::pow(2.f, bits);
        const int holdN = 1 + (int)(p.rust * p.rust * 60.f);
        const float crackP = p.crackle * p.crackle * 0.0025f * (192000.f / sr);
        const float outG = std::pow(10.f, p.outDb / 20.f);
        const float atk = 1.f - std::exp(-1.f / (0.001f * sr));
        const float rel = 1.f - std::exp(-1.f / (0.09f * sr));
        const int chatterN = std::max(1, (int)(sr / (1500.f + 5000.f * (1.f - p.gate))));

        for (int c = 0; c < std::min(numCh, (int)ch.size()); ++c) {
            Chan& s = ch[(size_t)c];
            // filtry zależne od parametrów
            s.pre.peak(sr, 850.f, 0.8f, 3.f + 7.f * p.drive);
            s.inHp.highpass(sr, 35.f, 0.7f);
            s.inter1.lowpass(sr, 7000.f, 0.7f);
            s.inter2.lowpass(sr, 5200.f - 1500.f * p.starve, 0.7f);
            s.fbHp.highpass(sr, 280.f, 0.7f);
            s.crackBp.bandpass(sr, 3200.f, 1.3f);
            const float tilt = (p.tone - 0.5f) * 22.f;
            s.lowS.lowshelf(sr, 650.f, -tilt);
            s.highS.highshelf(sr, 900.f, tilt);
            s.post.lowpass(sr, 7000.f + 9000.f * p.tone, 0.7f);

            float* x = io[c];
            const size_t N = s.fb.size();
            for (int i = 0; i < n; ++i) {
                const float dry = x[i];
                float v = s.inHp.process(dry);

                // sprzężenie zwrotne
                if (fbAmt > 0.001f) {
                    size_t rp = (s.fbPos + N - (size_t)fbDelay) % N;
                    float fb = s.fbHp.process(s.fb[rp]);
                    v += fbAmt * std::tanh(fb * 1.6f);
                }

                // obwiednia
                float a = std::fabs(v);
                s.env += (a > s.env ? atk : rel) * (a - s.env);

                // co 32 próbki: samogłoska maski podąża za kostką
                if ((s.vowelCnt++ & 31) == 0 && p.mask > 0.001f) setVowel(s, clampf(p.vowel + p.mask * 0.35f * clampf(s.env * 5.f, 0.f, 1.f), 0.f, 1.f));

                // stopnie przesteru
                v = s.pre.process(v);
                v = std::tanh(g1 * v + bias) - std::tanh(bias);
                v = s.inter1.process(v);
                v *= g2;
                v = v > 0 ? v / (1.f + v) : v / (1.f + 0.45f * std::fabs(v)) * 0.8f;
                v = s.inter2.process(v);
                v *= g3;
                float hard = clampf(v, -rail, rail * (1.f - 0.3f * p.starve));
                float folded = std::sin(v * kPi * 0.5f * (1.f + fold * 3.f)) * rail;
                v = hard + fold * (folded - hard);
                v *= makeup;

                // bramka-rozpad (velcro / zdychająca bateria)
                if (gateOn) {
                    float open = clampf((s.env - gateThr) / (gateThr * 0.7f + 1e-6f), 0.f, 1.f);
                    if (++s.chat >= chatterN) { s.chat = 0; s.gateT = (rnd(s) < open * open) ? 1.f : 0.f; }
                    s.gateG += (s.gateT - s.gateG) * 0.35f;
                    v *= s.gateG;
                }

                // oktawa
                if (p.octave > 0.001f) {
                    float oct = std::fabs(v) * 2.f - 1.f;
                    oct = oct - s.octX + 0.999f * s.octY; s.octX = std::fabs(v) * 2.f - 1.f; s.octY = oct;
                    v += p.octave * (oct * 1.4f - v);
                }

                // maska (nosowe formanty)
                if (p.mask > 0.001f) {
                    float m = s.f1.process(v) * 3.2f + s.f2.process(v) * 2.2f;
                    m = std::tanh(m * 2.6f);
                    v += p.mask * (m - v);
                }

                // rdza
                if (p.rust > 0.001f) {
                    float cr = std::round(v * levels) / levels;
                    if (++s.holdCnt >= holdN) { s.holdCnt = 0; s.holdV = cr; }
                    float rv = holdN > 1 ? s.holdV : cr;
                    v += std::min(1.f, p.rust * 3.f) * (rv - v);
                }

                // trzaski (gęstsze przy mocnym graniu)
                if (crackP > 0.f) {
                    float imp = 0.f;
                    if (rnd(s) < crackP * (0.15f + clampf(s.env * 8.f, 0.f, 3.f))) imp = (rnd(s) * 2.f - 1.f) * (0.7f + p.crackle);
                    v += s.crackBp.process(imp) * 3.5f;
                }

                // zapis do linii sprzężenia
                s.fb[s.fbPos] = v;
                s.fbPos = (s.fbPos + 1) % N;

                // barwa
                v = s.lowS.process(v);
                v = s.highS.process(v);
                v = s.post.process(v);
                // DC
                float y = v - s.dcX + 0.9995f * s.dcY; s.dcX = v; s.dcY = y;
                y *= outG;
                if (!std::isfinite(y)) { y = 0.f; s.resetFilters(); }
                y = clampf(y, -2.f, 2.f);
                x[i] = dry + p.mix * (y - dry);
            }
        }
    }

private:
    struct Chan {
        Biquad inHp, pre, inter1, inter2, f1, f2, fbHp, crackBp, lowS, highS, post;
        std::vector<float> fb; size_t fbPos = 0;
        float env = 0, gateG = 1, gateT = 1, holdV = 0, octX = 0, octY = 0, dcX = 0, dcY = 0;
        int chat = 0, holdCnt = 0; uint32_t vowelCnt = 0, rng = 1;
        void resetFilters() {
            for (Biquad* b : {&inHp, &pre, &inter1, &inter2, &f1, &f2, &fbHp, &crackBp, &lowS, &highS, &post}) b->reset();
            std::fill(fb.begin(), fb.end(), 0.f); env = 0; dcX = dcY = 0; octX = octY = 0;
        }
    };

    static float rnd(Chan& s) {
        s.rng ^= s.rng << 13; s.rng ^= s.rng >> 17; s.rng ^= s.rng << 5;
        return (s.rng & 0xFFFFFF) / 16777216.f;
    }

    void setVowel(Chan& s, float v) {
        static const float F[5][2] = {{350, 700}, {500, 900}, {800, 1150}, {500, 1750}, {300, 2300}};
        float t = v * 4.f; int k = std::min(3, (int)t); float f = t - (float)k;
        float a = F[k][0] + (F[k + 1][0] - F[k][0]) * f, b = F[k][1] + (F[k + 1][1] - F[k][1]) * f;
        s.f1.bandpass(sr, a, 6.f);
        s.f2.bandpass(sr, b, 8.f);
    }

    float sr = 192000.f;
    std::vector<Chan> ch;
    Params smooth;
    bool first = true;
};

} // namespace stonoga
