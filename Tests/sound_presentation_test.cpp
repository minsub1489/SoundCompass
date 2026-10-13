#include "../Source/SoundCompass/SoundVisualization/SoundPresentation.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace SoundCompassPresentation;

int Checks = 0;
void Check(bool Condition, const char* Message)
{
    ++Checks;
    if (!Condition) { std::cerr << "FAILED: " << Message << '\n'; std::exit(1); }
}

bool Near(float A, float B) { return std::abs(A - B) < 0.00001f; }

int main()
{
    constexpr float Hold = 0.6f, Fade = 0.8f, Floor = 0.001f;
    EnvelopeTrail Pulse;
    Check(Pulse.Visibility(0, Hold, Fade) == 0, "unplayed sources stay hidden");
    Check(!Pulse.Observe(0, 0, Floor, Hold, Fade), "silence cannot create a marker");
    Check(!Pulse.Observe(Floor * 0.5f, 0, Floor, Hold, Fade), "inaudible signal cannot create a marker");
    Check(!Pulse.Observe(std::numeric_limits<float>::quiet_NaN(), 0, Floor, Hold, Fade), "invalid envelope ignored");
    Check(Pulse.Observe(0.5f, 10, Floor, Hold, Fade), "audible pulse captured at the callback");
    Pulse.Observe(0, 10.001, Floor, Hold, Fade);
    Check(Pulse.Visibility(10.033, Hold, Fade) == 1, "pulse ending between HUD refreshes survives");
    Check(Pulse.Count == 1 && Pulse.Sample(0) == 0.5f, "silence preserves captured waveform");
    Check(Pulse.Visibility(10.59, Hold, Fade) == 1, "short sound remains fully visible throughout hold");
    Check(Near(Pulse.Visibility(11, Hold, Fade), 0.5f), "halfway through fade both size and opacity halve");
    Check(Pulse.Visibility(11.41, Hold, Fade) == 0, "marker expires after hold plus fade");
    bool Monotonic = true;
    float Previous = 1;
    for (int Step = 0; Step <= 100; ++Step)
    {
        const float Weight = Pulse.Visibility(10.6 + Step * 0.008, Hold, Fade);
        Monotonic &= Weight <= Previous + 0.00001f && Weight >= 0 && Weight <= 1;
        Previous = Weight;
    }
    Check(Monotonic, "release never grows or flickers back into view");
    Check(Retention(0.601, Hold, Fade) > 0.999f, "release starts smoothly without a jump");
    Check(Retention(1.399, Hold, Fade) < 0.001f, "release ends smoothly without a jump");

    Pulse.Observe(0.25f, 11.1, Floor, Hold, Fade);
    Check(Pulse.Visibility(11.1, Hold, Fade) == 1, "new audible event immediately restores full visibility");
    Check(Pulse.Visibility(11.69, Hold, Fade) == 1, "new event restarts the hold timer");
    Pulse.Observe(0.1f, 13, Floor, Hold, Fade);
    Check(Pulse.Count == 1 && Pulse.Sample(0) == 0.1f, "expired event does not contaminate a new trail");

    EnvelopeTrail Tail;
    Tail.Observe(0.5f, 0, Floor, Hold, Fade);
    Tail.Observe(Floor, 0.1, Floor, Hold, Fade);
    Check(Tail.HeldEnvelope == 0.5f, "quiet final callback cannot erase a short transient's peak");
    Tail.Observe(0.01f, 2, Floor, Hold, Fade);
    Check(Near(Tail.HeldEnvelope, 0.01f), "sustained quieter audio eventually lowers the display level");
    EnvelopeTrail Quiet;
    Quiet.Observe(Floor, 0, Floor, Hold, Fade);
    Check(Near(Quiet.Visibility(1, Hold, Fade), 0.5f), "threshold-level sound receives the full fade duration");
    Check(Quiet.HeldEnvelope == Floor, "fade does not alter the level used by importance selection");

    EnvelopeTrail Other;
    Other.Observe(0.8f, 10, Floor, Hold, Fade);
    Check(Pulse.HeldEnvelope == 0.1f && Other.HeldEnvelope == 0.8f, "simultaneous sources have independent histories");
    EnvelopeTrail Loop;
    for (int Index = 0; Index < 200; ++Index) { Loop.Observe(0.01f + Index * 0.001f, Index * 0.01, Floor, Hold, Fade); }
    Check(Loop.Count == TrailSize, "looping source memory remains bounded");
    Check(Near(Loop.Sample(0), 0.146f) && Near(Loop.Sample(TrailSize - 1), 0.209f), "wrapped history retains chronological order");
    Check(Near(Loop.Visibility(2.5, Hold, Fade), 1), "active loop keeps refreshing retention");
    Check(Near(Retention(0.4, 0, Fade), 0.5f), "zero hold starts an immediate gradual fade");
    Check(Retention(0.61, Hold, 0) == 0, "zero fade removes at end of hold without division by zero");
    Check(Retention(0.01, -1, -1) == 0, "negative durations safely clamp to zero");

    Check(ArcAngleDegrees(90, 50, 0) == 65 && ArcAngleDegrees(90, 50, 1) == 115,
        "right-hand sound occupies only the configured 50 degree sector");
    Check(ArcAngleDegrees(-90, 50, 0.5f) == -90, "left-hand sector stays centred on its source");
    Check(ArcAngleDegrees(179, 50, 0) == 154 && ArcAngleDegrees(179, 50, 1) == 204,
        "rear sector crosses the angle seam without wrapping around the entire ring");
    Check(ArcAngleDegrees(0, 360, 1) - ArcAngleDegrees(0, 360, 0) == 120,
        "invalid full-circle setting cannot reintroduce the 360 degree waveform");
    Check(Near(ArcEdgeWeight(0), 0) && Near(ArcEdgeWeight(1), 0) && Near(ArcEdgeWeight(0.5f), 1),
        "waveform tapers into both sector ends");
    std::cout << Checks << " presentation checks passed\n";
}
