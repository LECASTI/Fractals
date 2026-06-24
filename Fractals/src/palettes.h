#ifndef PALETTES_H
#define PALETTES_H

#include <cmath>

// ponytail: structures and procedural generator for 50 distinct color palettes with names.
struct Palette {
    float a[3];   // bias
    float b[3];   // scale
    float c[3];   // frequency
    float d[3];   // phase
    float bg[3];  // background color (skybox)
    const char* name; // Display name
};

inline Palette GetPalette(int index) {
    // 10 base configuration sets
    static const float baseA[10][3] = {
        {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.8f, 0.5f, 0.4f}, {0.5f, 0.5f, 0.5f}, {0.8f, 0.5f, 0.4f},
        {0.5f, 0.5f, 0.5f}, {0.8f, 0.5f, 0.4f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.8f, 0.8f, 0.5f}
    };
    static const float baseB[10][3] = {
        {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.2f, 0.4f, 0.2f}, {0.5f, 0.5f, 0.5f}, {0.2f, 0.4f, 0.2f},
        {0.5f, 0.5f, 0.5f}, {0.2f, 0.4f, 0.2f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.2f, 0.2f, 0.5f}
    };
    static const float baseC[10][3] = {
        {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {2.0f, 1.0f, 1.0f}, {2.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 2.0f}, {1.0f, 1.0f, 0.5f}, {1.0f, 0.7f, 0.4f}, {2.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}
    };
    static const float baseD[10][3] = {
        {0.00f, 0.33f, 0.67f}, {0.00f, 0.10f, 0.20f}, {0.00f, 0.25f, 0.25f}, {0.50f, 0.20f, 0.25f}, {0.00f, 0.10f, 0.20f},
        {0.00f, 0.33f, 0.67f}, {0.00f, 0.10f, 0.20f}, {0.00f, 0.15f, 0.20f}, {0.00f, 0.35f, 0.50f}, {0.00f, 0.40f, 0.80f}
    };

    static const char* baseNames[10] = {
        "Classic Rainbow", "Neon Dreams", "Sunset Sunset", "Cyber Core", "Copper Dust",
        "Deep Marine", "Electric Desert", "Soft Pastels", "Magma Flow", "Aurora Borealis"
    };

    index = (index < 0 ? 0 : index) % 50;

    int baseIdx = index % 10;
    float shift = static_cast<float>(index / 10) * 0.15f;

    Palette p;
    for (int i = 0; i < 3; ++i) {
        p.a[i] = baseA[baseIdx][i];
        p.b[i] = baseB[baseIdx][i];
        p.c[i] = baseC[baseIdx][i];
        p.d[i] = baseD[baseIdx][i] + shift;
        if (p.d[i] > 1.0f) p.d[i] -= 1.0f;
    }

    p.bg[0] = 0.01f + 0.02f * std::sin(static_cast<float>(index) * 1.3f);
    p.bg[1] = 0.01f + 0.015f * std::cos(static_cast<float>(index) * 0.9f);
    p.bg[2] = 0.015f + 0.02f * std::sin(static_cast<float>(index) * 0.7f);

    for (int i = 0; i < 3; ++i) {
        if (p.bg[i] < 0.005f) p.bg[i] = 0.005f;
        if (p.bg[i] > 0.05f) p.bg[i] = 0.05f;
    }

    p.name = baseNames[baseIdx];
    return p;
}

#endif
