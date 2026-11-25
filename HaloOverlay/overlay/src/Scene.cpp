#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "HaloMath.h"
#include "SoundImportancePolicy.h"

namespace halo
{
float BinCentreMeters(DistBin bin)
{
    switch (bin)
    {
    case DistBin::Near: return 2.5f;
    case DistBin::Mid: return 10.0f;
    case DistBin::Far: return 30.0f;
    }
    return 10.0f;
}

Scene::Scene(SceneConfig config) : config_(std::move(config)) {}

float Scene::Alpha(const Track& track, double now) const
{
    return SoundCompassPresentation::Retention(now - track.lastSeen, config_.holdSeconds, config_.fadeSeconds);
}

void Scene::Apply(const Message& message, double now)
{
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(), [&](const Track& t) { return Alpha(t, now) <= 0.0f; }),
        tracks_.end());
    if (message.src != config_.srcFilter) { return; }

    for (const Event& event : message.events)
    {
        auto same = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& t) { return t.senderId == event.id; });
        if (same == tracks_.end())
        {
            same = std::find_if(tracks_.begin(), tracks_.end(), [&](const Track& t)
            {
                return t.last.cls == event.cls && std::fabs(AngleDiff(t.last.az, event.az)) <= config_.mergeDegrees;
            });
        }
        if (same == tracks_.end())
        {
            Track track;
            track.key = nextKey_++;
            track.onset = now;
            tracks_.push_back(track);
            same = tracks_.end() - 1;
        }
        same->senderId = event.id;
        same->last = event;
        same->lastSeen = now;
        same->trail.Observe(event.level, now, 0.0f, config_.holdSeconds, config_.fadeSeconds);
    }
}

std::vector<Mark> Scene::Snapshot(double now) const
{
    struct Ranked { const Track* track; float alpha; float importance; };
    std::vector<Ranked> ranked;
    for (const Track& track : tracks_)
    {
        const float alpha = Alpha(track, now);
        if (alpha <= 0.0f) { continue; }
        std::optional<float> distance = track.last.distM;
        if (!distance && track.last.distBin) { distance = BinCentreMeters(*track.last.distBin); }
        const float db = 20.0f * std::log10(std::max(track.last.level, 1e-4f));
        const float importance = SoundCompassImportance::Score(StyleOf(track.last.cls).baseImportance,
            distance ? *distance : std::numeric_limits<float>::quiet_NaN(), db);
        ranked.push_back({&track, alpha, importance});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) { return a.importance > b.importance; });

    std::vector<Mark> marks;
    int iconSlots = 0; // the icon cap counts icons on screen, so a mirrored track uses two
    for (size_t index = 0; index < ranked.size(); ++index)
    {
        const Track& track = *ranked[index].track;
        Mark mark;
        mark.track = track.key;
        mark.cls = track.last.cls;
        mark.az = track.last.az;
        mark.distM = track.last.distM;
        if (!mark.distM && track.last.distBin) { mark.distM = BinCentreMeters(*track.last.distBin); }
        mark.elevM = track.last.elevM;
        mark.alpha = ranked[index].alpha;
        mark.onsetAge = static_cast<float>(now - track.onset);
        mark.importance = ranked[index].importance;
        const bool mirrored = track.last.fb < config_.mirrorBelowFb;
        mark.marker = iconSlots + (mirrored ? 2 : 1) <= config_.maxMarkers;
        if (mark.marker) { iconSlots += mirrored ? 2 : 1; }
        mark.trail = track.trail;
        if (mirrored)
        {
            // Front/back unknown: show both candidates, each at half strength.
            mark.mirrored = true;
            mark.alpha *= 0.5f;
            Mark back = mark;
            back.az = static_cast<float>(AngleDiff(180.0 - mark.az, 0.0));
            marks.push_back(mark);
            marks.push_back(back);
        }
        else
        {
            marks.push_back(mark);
        }
    }
    return marks;
}

bool Scene::Idle(double now) const
{
    return std::none_of(tracks_.begin(), tracks_.end(), [&](const Track& t) { return Alpha(t, now) > 0.0f; });
}
}
