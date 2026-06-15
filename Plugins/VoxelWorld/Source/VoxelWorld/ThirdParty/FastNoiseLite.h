#pragma once
// Minimal Perlin-FBm noise with FastNoiseLite-compatible API
// Algorithm: Ken Perlin "Improving Noise" SIGGRAPH 2002
// Used by: MapGenerator3D (terrain heightmap + cave carving)

#include <cmath>

class FastNoiseLite
{
public:
    enum class NoiseType   { Perlin };
    enum class FractalType { None, FBm };

    FastNoiseLite() { SetSeed(0); }

    void SetSeed(int Seed)
    {
        for (int i = 0; i < 256; ++i) P[i] = i;
        unsigned S = (unsigned)(Seed ^ 0xDEADBEEF);
        for (int i = 255; i > 0; --i)
        {
            S = S * 1664525u + 1013904223u;
            int j = (int)(S & 0x7FFFFFFFu) % (i + 1);
            int t = P[i]; P[i] = P[j]; P[j] = t;
        }
        for (int i = 0; i < 256; ++i) P[256 + i] = P[i];
    }

    void SetNoiseType(NoiseType)       {}
    void SetFrequency(float F)         { Freq = F; }
    void SetFractalType(FractalType T) { Fractal = T; }
    void SetFractalOctaves(int O)      { Octaves = O; }
    void SetFractalLacunarity(float L) { Lacunarity = L; }
    void SetFractalGain(float G)       { Gain = G; }

    float GetNoise(float X, float Y) const
    {
        if (Fractal == FractalType::FBm)
        {
            float v = 0.f, a = 1.f, f = Freq, s = 0.f;
            for (int i = 0; i < Octaves; ++i)
            { v += Perlin2(X*f, Y*f)*a; s += a; a *= Gain; f *= Lacunarity; }
            return v / s;
        }
        return Perlin2(X * Freq, Y * Freq);
    }

    float GetNoise(float X, float Y, float Z) const
    {
        if (Fractal == FractalType::FBm)
        {
            float v = 0.f, a = 1.f, f = Freq, s = 0.f;
            for (int i = 0; i < Octaves; ++i)
            { v += Perlin3(X*f, Y*f, Z*f)*a; s += a; a *= Gain; f *= Lacunarity; }
            return v / s;
        }
        return Perlin3(X * Freq, Y * Freq, Z * Freq);
    }

private:
    int         P[512];
    float       Freq       = 0.01f;
    FractalType Fractal    = FractalType::None;
    int         Octaves    = 1;
    float       Lacunarity = 2.f;
    float       Gain       = 0.5f;

    static float Fade(float t) { return t*t*t*(t*(t*6.f - 15.f) + 10.f); }
    static float Lerp(float a, float b, float t) { return a + t*(b-a); }

    float Grad2(int H, float X, float Y) const
    {
        switch (H & 3) {
            case 0: return  X + Y;
            case 1: return -X + Y;
            case 2: return  X - Y;
            default: return -X - Y;
        }
    }

    float Perlin2(float X, float Y) const
    {
        int xi = (int)floorf(X) & 255, yi = (int)floorf(Y) & 255;
        float dx = X - floorf(X), dy = Y - floorf(Y);
        float u = Fade(dx), v = Fade(dy);
        int A = P[xi]+yi,   B = P[xi+1]+yi;
        return Lerp(
            Lerp(Grad2(P[A],   dx,   dy),   Grad2(P[B],   dx-1, dy),   u),
            Lerp(Grad2(P[A+1], dx,   dy-1), Grad2(P[B+1], dx-1, dy-1), u), v);
    }

    float Grad3(int H, float X, float Y, float Z) const
    {
        int h = H & 15;
        float u = h < 8 ? X : Y;
        float v = h < 4 ? Y : (h==12||h==14 ? X : Z);
        return ((h&1)?-u:u) + ((h&2)?-v:v);
    }

    float Perlin3(float X, float Y, float Z) const
    {
        int xi = (int)floorf(X) & 255, yi = (int)floorf(Y) & 255, zi = (int)floorf(Z) & 255;
        float dx = X-floorf(X), dy = Y-floorf(Y), dz = Z-floorf(Z);
        float u = Fade(dx), v = Fade(dy), w = Fade(dz);
        int A  = P[xi]+yi,   AA = P[A]+zi,   AB = P[A+1]+zi;
        int B  = P[xi+1]+yi, BA = P[B]+zi,   BB = P[B+1]+zi;
        return Lerp(
            Lerp(Lerp(Grad3(P[AA],   dx,   dy,   dz  ), Grad3(P[BA],   dx-1, dy,   dz  ), u),
                 Lerp(Grad3(P[AB],   dx,   dy-1, dz  ), Grad3(P[BB],   dx-1, dy-1, dz  ), u), v),
            Lerp(Lerp(Grad3(P[AA+1], dx,   dy,   dz-1), Grad3(P[BA+1], dx-1, dy,   dz-1), u),
                 Lerp(Grad3(P[AB+1], dx,   dy-1, dz-1), Grad3(P[BB+1], dx-1, dy-1, dz-1), u), v), w);
    }
};
