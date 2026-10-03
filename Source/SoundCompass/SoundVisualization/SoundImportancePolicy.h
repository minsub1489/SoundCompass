#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Project-specific experimental policy, not a scoring formula from the reference paper.
namespace SoundCompassImportance
{
struct Weights
{
    float Semantic = 0.70f;
    float Proximity = 0.20f;
    float Loudness = 0.10f;
    float DistanceRangeMeters = 20.0f;
    float AudibleFloorDb = -60.0f;
};

inline float Unit(float Value)
{
    return std::isfinite(Value) ? std::clamp(Value, 0.0f, 1.0f) : 0.0f;
}

inline float Score(float BaseImportance, float DistanceMeters, float ListenerDb, const Weights& W = {})
{
    const float SemanticWeight = Unit(W.Semantic);
    const float DistanceWeight = Unit(W.Proximity);
    const float LoudnessWeight = Unit(W.Loudness);
    const float Sum = SemanticWeight + DistanceWeight + LoudnessWeight;
    if (Sum <= 0.0f) { return Unit(BaseImportance); }
    const float Range = std::isfinite(W.DistanceRangeMeters) ? std::max(0.01f, W.DistanceRangeMeters) : 20.0f;
    const float Distance = std::isfinite(DistanceMeters) ? std::max(0.0f, DistanceMeters) : Range;
    const float Floor = std::isfinite(W.AudibleFloorDb) ? std::min(-0.01f, W.AudibleFloorDb) : -60.0f;
    const float Proximity = Unit(1.0f - Distance / Range);
    const float Loudness = Unit((ListenerDb - Floor) / -Floor);
    return Unit((SemanticWeight * Unit(BaseImportance) + DistanceWeight * Proximity +
        LoudnessWeight * Loudness) / Sum);
}

struct Candidate
{
    int Id = 0;
    float Importance = 0.0f;
};

// Returns original array indices; equal scores use the playback ID for deterministic order.
// The caller has already removed silent/paused/virtualised sources in BOTH conditions.
inline std::vector<int> Select(const std::vector<Candidate>& Candidates, float Threshold, int Limit)
{
    std::vector<int> Indices;
    if (Limit <= 0) { return Indices; }
    for (int i = 0; i < static_cast<int>(Candidates.size()); ++i)
    {
        if (std::isfinite(Candidates[i].Importance) && Candidates[i].Importance >= Unit(Threshold))
        {
            Indices.push_back(i);
        }
    }
    std::sort(Indices.begin(), Indices.end(), [&](int A, int B)
    {
        return Candidates[A].Importance == Candidates[B].Importance ? Candidates[A].Id < Candidates[B].Id :
            Candidates[A].Importance > Candidates[B].Importance;
    });
    if (Indices.size() > static_cast<std::size_t>(Limit)) { Indices.resize(Limit); }
    return Indices;
}
}
