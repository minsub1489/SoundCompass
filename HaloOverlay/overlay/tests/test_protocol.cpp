#include "Check.h"
#include "Protocol.h"

using namespace halo;

static const char* kExample = R"({
  "v": 1, "src": "model", "mode": "stereo", "t": 1234.567,
  "events": [ { "id": 17, "cls": "Gunshot", "conf": 0.82, "az": 35.0, "fb": 0.2,
                "dist_m": null, "dist_bin": "mid", "dist_src": "level", "elev_m": null, "level": 0.7 } ]
})";

static void parses_full_event()
{
    const auto m = ParseMessage(kExample);
    CHECK(m.has_value());
    if (!m) { return; }
    CHECK(m->src == "model");
    CHECK(m->mode == "stereo");
    CHECK(Near(m->t, 1234.567));
    CHECK(m->events.size() == 1);
    if (m->events.size() != 1) { return; }
    const Event& e = m->events[0];
    CHECK(e.id == 17);
    CHECK(e.cls == "Gunshot");
    CHECK(e.az == 35.0f);
    CHECK(e.fb == 0.2f);
    CHECK(!e.distM);
    CHECK(e.distBin == DistBin::Mid);
    CHECK(!e.elevM);
    CHECK(e.level == 0.7f);
    CHECK(e.conf == 0.82f);
}

static void null_fields_are_empty()
{
    const auto m = ParseMessage(R"({"v":1,"src":"ue","mode":"ground_truth","t":0,
        "events":[{"id":1,"cls":"Door","conf":1,"az":0,"fb":1,"dist_m":3.5,"dist_bin":null,"elev_m":-1,"level":0.5}]})");
    CHECK(m && m->events.size() == 1);
    if (!m || m->events.empty()) { return; }
    CHECK(!m->events[0].distBin);
    CHECK(m->events[0].distM && *m->events[0].distM == 3.5f);
    CHECK(m->events[0].elevM && *m->events[0].elevM == -1.0f);
}

static void malformed_is_ignored()
{
    CHECK(!ParseMessage("{"));
    CHECK(!ParseMessage("[]"));
    CHECK(!ParseMessage(R"({"v":2,"src":"model","mode":"stereo","t":0,"events":[]})"));
    CHECK(!ParseMessage(R"({"v":1,"src":"model","mode":"stereo","t":0,"events":{}})"));
    const auto bad = ParseMessage(R"({"v":1,"src":"model","mode":"stereo","t":0,
        "events":[{"id":1,"cls":"Door","conf":1,"az":"left","fb":1,"level":0.5}]})");
    CHECK(bad && bad->events.empty()); // only the broken event is dropped
}

static void unknown_class_dropped()
{
    const auto m = ParseMessage(R"({"v":1,"src":"model","mode":"stereo","t":0,"events":[
        {"id":1,"cls":"Banana","conf":1,"az":0,"fb":0,"level":0.5},
        {"id":2,"cls":"Voice","conf":1,"az":10,"fb":0,"level":0.5}]})");
    CHECK(m && m->events.size() == 1 && m->events[0].cls == "Voice");
}

static void empty_events_ok()
{
    const auto m = ParseMessage(R"({"v":1,"src":"model","mode":"surround","t":5,"events":[]})");
    CHECK(m && m->events.empty() && m->mode == "surround");
}

int main()
{
    RUN(parses_full_event);
    RUN(null_fields_are_empty);
    RUN(malformed_is_ignored);
    RUN(unknown_class_dropped);
    RUN(empty_events_ok);
    return g_failures;
}
