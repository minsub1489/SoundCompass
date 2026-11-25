#include "Check.h"
#include "Scene.h"

#include <algorithm>

using namespace halo;

static Event Ev(int id, const char* cls, float az, float fb = 1.0f, float level = 0.5f)
{
    Event e;
    e.id = id;
    e.cls = cls;
    e.az = az;
    e.fb = fb;
    e.level = level;
    return e;
}

static Message Msg(std::vector<Event> events, const char* src = "model")
{
    Message m;
    m.src = src;
    m.mode = "stereo";
    m.events = std::move(events);
    return m;
}

static void event_appears_then_settles()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Gunshot", 10)}), 0.0);
    const auto marks = scene.Snapshot(0.0);
    CHECK(marks.size() == 1);
    if (marks.size() != 1) { return; }
    CHECK(marks[0].alpha > 0.99f);
    CHECK(marks[0].onsetAge == 0.0f);
    CHECK(marks[0].cls == "Gunshot");
}

static void sender_stops_marks_fade_out()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Gunshot", 10)}), 0.0);
    const auto held = scene.Snapshot(0.5);
    CHECK(held.size() == 1 && Near(held[0].alpha, 1.0));
    const auto fading = scene.Snapshot(1.0);
    CHECK(fading.size() == 1 && fading[0].alpha > 0.0f && fading[0].alpha < 1.0f);
    CHECK(scene.Snapshot(1.41).empty());
    CHECK(scene.Idle(1.41));
    CHECK(!scene.Idle(1.0));
}

static void same_id_is_one_track()
{
    Scene scene;
    for (int i = 0; i < 5; ++i) { scene.Apply(Msg({Ev(7, "Footstep", 40.0f + i)}), 0.1 * i); }
    const auto marks = scene.Snapshot(0.4);
    CHECK(marks.size() == 1);
    CHECK(!marks.empty() && Near(marks[0].onsetAge, 0.4, 1e-3));
}

static void merge_across_wrap()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Door", 179)}), 0.0);
    scene.Apply(Msg({Ev(2, "Door", -179)}), 0.1);
    CHECK(scene.Snapshot(0.1).size() == 1);
}

static void different_class_not_merged()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Gunshot", 20), Ev(2, "Door", 20)}), 0.0);
    CHECK(scene.Snapshot(0.0).size() == 2);
}

static void mirror_when_front_back_unknown()
{
    Scene trusted;
    trusted.Apply(Msg({Ev(1, "Voice", 30, 1.0f)}), 0.0);
    const float full = trusted.Snapshot(0.0).at(0).alpha;

    Scene scene;
    scene.Apply(Msg({Ev(1, "Voice", 30, 0.2f)}), 0.0);
    auto marks = scene.Snapshot(0.0);
    CHECK(marks.size() == 2);
    if (marks.size() != 2) { return; }
    std::sort(marks.begin(), marks.end(), [](const Mark& a, const Mark& b) { return a.az < b.az; });
    CHECK(Near(marks[0].az, 30.0) && Near(marks[1].az, 150.0));
    CHECK(marks[0].mirrored && marks[1].mirrored);
    CHECK(Near(marks[0].alpha, 0.5 * full) && Near(marks[1].alpha, 0.5 * full));

    Scene left;
    left.Apply(Msg({Ev(1, "Voice", -30, 0.2f)}), 0.0);
    bool hasBackLeft = false;
    for (const Mark& m : left.Snapshot(0.0)) { hasBackLeft |= Near(m.az, -150.0); }
    CHECK(hasBackLeft);

    Scene sure;
    sure.Apply(Msg({Ev(1, "Voice", 30, 0.8f)}), 0.0);
    CHECK(sure.Snapshot(0.0).size() == 1);
}

static void bin_sets_distance()
{
    Event far = Ev(1, "Gunshot", 0);
    far.distBin = DistBin::Far;
    Event none = Ev(2, "Door", 90);
    Event meters = Ev(3, "Voice", -90);
    meters.distM = 7.0f;
    meters.distBin = DistBin::Near;
    Scene scene;
    scene.Apply(Msg({far, none, meters}), 0.0);
    for (const Mark& m : scene.Snapshot(0.0))
    {
        if (m.cls == "Gunshot") { CHECK(m.distM && Near(*m.distM, 30.0)); }
        if (m.cls == "Door") { CHECK(!m.distM); }
        if (m.cls == "Voice") { CHECK(m.distM && Near(*m.distM, 7.0)); }
    }
    CHECK(Near(BinCentreMeters(DistBin::Near), 2.5) && Near(BinCentreMeters(DistBin::Mid), 10.0));
}

static void many_events_cap_markers()
{
    static const char* classes[] = {"Alarm", "Dog", "Door", "Explosion", "Fire", "Footstep", "GlassBreak",
        "Gunshot", "Impact", "Item", "Reload", "Throwable", "Vehicle", "Voice"};
    std::vector<Event> events;
    for (int i = 0; i < 30; ++i) { events.push_back(Ev(i + 1, classes[i % 14], -180.0f + 12.0f * i)); }
    Scene scene;
    scene.Apply(Msg(events), 0.0);
    const auto marks = scene.Snapshot(0.0);
    CHECK(marks.size() == 30);
    const auto markers = std::count_if(marks.begin(), marks.end(), [](const Mark& m) { return m.marker; });
    CHECK(markers == 4);
    float lowestMarker = 1.0f, highestOther = 0.0f;
    for (const Mark& m : marks)
    {
        if (m.marker) { lowestMarker = std::min(lowestMarker, m.importance); }
        else { highestOther = std::max(highestOther, m.importance); }
    }
    CHECK(lowestMarker >= highestOther);
}

static void src_filter()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Door", 0)}, "ue"), 0.0);
    CHECK(scene.Snapshot(0.0).empty());
    SceneConfig config;
    config.srcFilter = "ue";
    Scene ue(config);
    ue.Apply(Msg({Ev(1, "Door", 0)}, "ue"), 0.0);
    CHECK(ue.Snapshot(0.0).size() == 1);
}

static void level_feeds_trail()
{
    Scene scene;
    scene.Apply(Msg({Ev(1, "Footstep", 0, 1.0f, 0.2f)}), 0.0);
    scene.Apply(Msg({Ev(1, "Footstep", 0, 1.0f, 0.8f)}), 0.1);
    scene.Apply(Msg({Ev(1, "Footstep", 0, 1.0f, 0.5f)}), 0.2);
    const auto marks = scene.Snapshot(0.2);
    CHECK(marks.size() == 1);
    if (marks.empty()) { return; }
    CHECK(marks[0].trail.Count == 3);
    CHECK(marks[0].trail.PeakEnvelope == 0.8f);
}

int main()
{
    RUN(event_appears_then_settles);
    RUN(sender_stops_marks_fade_out);
    RUN(same_id_is_one_track);
    RUN(merge_across_wrap);
    RUN(different_class_not_merged);
    RUN(mirror_when_front_back_unknown);
    RUN(bin_sets_distance);
    RUN(many_events_cap_markers);
    RUN(src_filter);
    RUN(level_feeds_trail);
    return g_failures;
}
