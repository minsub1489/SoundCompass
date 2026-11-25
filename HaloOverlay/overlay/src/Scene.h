#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Classes.h"
#include "Protocol.h"
#include "SoundPresentation.h"

namespace halo
{
/** Where a pointer stands for a distance bin: the middle of near (<5 m), mid (5-15 m), far (15 m+). */
float BinCentreMeters(DistBin bin);

struct SceneConfig
{
    std::string srcFilter = "model";
    float holdSeconds = 0.60f;
    float fadeSeconds = 0.80f;
    int maxMarkers = 4;
    float mergeDegrees = 20.0f;
    float mirrorBelowFb = 0.5f;
};

/** One thing to draw. A track whose front/back is unknown becomes two mirrored Marks. */
struct Mark
{
    int track = 0;
    std::string cls;
    float az = 0.0f;
    std::optional<float> distM;
    std::optional<float> elevM;
    float alpha = 0.0f;
    float onsetAge = 0.0f;
    float importance = 0.0f;
    bool mirrored = false;
    bool marker = false;     // gets an edge icon (the most important tracks only)
    SoundCompassPresentation::EnvelopeTrail trail;
};

/** Presentation state over time: merging, hold and fade, importance. The senders only report what is audible now. */
class Scene
{
public:
    explicit Scene(SceneConfig config = {});
    void Apply(const Message& message, double now);
    std::vector<Mark> Snapshot(double now) const;
    bool Idle(double now) const;

private:
    struct Track
    {
        int key = 0;
        int senderId = 0;
        Event last;
        double onset = 0.0;
        double lastSeen = 0.0;
        SoundCompassPresentation::EnvelopeTrail trail;
    };

    float Alpha(const Track& track, double now) const;

    SceneConfig config_;
    std::vector<Track> tracks_;
    int nextKey_ = 1;
};
}
