#pragma once

#include <algorithm>
#include <array>
#include <cmath>

/** Presentation only: retained measurements never change audio playback. */
namespace SoundCompassPresentation
{
constexpr int TrailSize = 64;

inline float Retention(double Age, float HoldSeconds, float FadeSeconds)
{
    const double Hold = std::max(0.0f, HoldSeconds);
    const double Fade = std::max(0.0f, FadeSeconds);
    if (Age <= Hold) { return 1.0f; }
    if (Fade <= 0.0 || Age >= Hold + Fade) { return 0.0f; }
    const float T = static_cast<float>((Age - Hold) / Fade);
    return 1.0f - T * T * (3.0f - 2.0f * T);
}

/** Latch in the envelope callback so a pulse between HUD refreshes is preserved. */
struct EnvelopeTrail
{
    std::array<float, TrailSize> Samples{};
    int Count = 0;
    int WriteIndex = 0;
    float PeakEnvelope = 0.0f;
    float HeldEnvelope = 0.0f;
    double PeakTime = -1.0;
    double LastAudibleTime = -1.0;

    float Visibility(double Now, float Hold, float Fade) const
    {
        return LastAudibleTime < 0.0 ? 0.0f : Retention(Now - LastAudibleTime, Hold, Fade);
    }

    bool Observe(float Value, double Now, float Floor, float Hold, float Fade)
    {
        if (!std::isfinite(Value) || Value <= 0.0f || Value < Floor) { return false; }
        if (Visibility(Now, Hold, Fade) <= 0.0f)
        {
            Count = WriteIndex = 0;
            PeakEnvelope = 0.0f;
        }
        const float RetainedPeak = PeakEnvelope * Retention(Now - PeakTime, Hold, Fade);
        if (Value >= RetainedPeak)
        {
            PeakEnvelope = Value;
            PeakTime = Now;
        }
        HeldEnvelope = std::max(Value, RetainedPeak);
        LastAudibleTime = Now;
        Samples[WriteIndex] = Value;
        WriteIndex = (WriteIndex + 1) % TrailSize;
        Count = std::min(Count + 1, TrailSize);
        return true;
    }

    float Sample(int Index) const
    {
        if (Count == 0) { return 0.0f; }
        const int Start = (WriteIndex - Count + TrailSize) % TrailSize;
        return Samples[(Start + std::clamp(Index, 0, Count - 1)) % TrailSize];
    }
};

inline float ArcAngleDegrees(float Azimuth, float Width, float Fraction)
{
    return Azimuth + (std::clamp(Fraction, 0.0f, 1.0f) - 0.5f) * std::clamp(Width, 10.0f, 120.0f);
}

inline float ArcEdgeWeight(float Fraction)
{
    constexpr double Pi = 3.14159265358979323846;
    const double Sine = std::sin(Pi * std::clamp(Fraction, 0.0f, 1.0f));
    return static_cast<float>(Sine * Sine);
}
}
