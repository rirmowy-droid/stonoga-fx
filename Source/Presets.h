#pragma once
#include "StonogaDSP.h"

namespace stonoga {

struct Preset { const char* name; Params p; };

// drive, starve, gate, octave, mask, vowel, rust, crackle, feedback, fbTime, tone, mix, outDb
inline const Preset kPresets[] = {
    {"Mieso",            {0.62f, 0.00f, 0.00f, 0.00f, 0.00f, 0.5f, 0.00f, 0.00f, 0.00f, 0.3f, 0.45f, 1.f, -10.f}},
    {"Zdychajaca bateria",{0.78f, 0.75f, 0.35f, 0.00f, 0.00f, 0.5f, 0.10f, 0.15f, 0.00f, 0.3f, 0.40f, 1.f, -7.f}},
    {"Maska no",         {0.70f, 0.10f, 0.00f, 0.15f, 0.85f, 0.35f, 0.00f, 0.00f, 0.00f, 0.3f, 0.55f, 1.f, -8.f}},
    {"Rdza",             {0.85f, 0.20f, 0.10f, 0.30f, 0.00f, 0.5f, 0.60f, 0.45f, 0.00f, 0.3f, 0.60f, 1.f, -8.f}},
    {"Sprzezenie",       {0.90f, 0.10f, 0.00f, 0.20f, 0.20f, 0.6f, 0.10f, 0.10f, 0.55f, 0.25f, 0.55f, 1.f, -7.f}},
    {"Totalny rozpad",   {1.00f, 0.60f, 0.55f, 0.50f, 0.50f, 0.7f, 0.75f, 0.80f, 0.45f, 0.65f, 0.65f, 1.f, -3.f}},
};
inline constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace stonoga
