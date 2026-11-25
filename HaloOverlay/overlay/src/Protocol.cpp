#include "Protocol.h"

#include <array>

#include <nlohmann/json.hpp>

namespace halo
{
namespace
{
using nlohmann::json;

constexpr std::array<std::string_view, 15> kClasses = {
    "Alarm", "Dog", "Door", "Explosion", "Fire", "Footstep", "GlassBreak", "Gunshot",
    "Impact", "Item", "Reload", "Throwable", "Vehicle", "Voice", "Other"};

// Required number; false when missing or not a number.
bool ReadNumber(const json& object, const char* key, float& out)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) { return false; }
    out = it->get<float>();
    return true;
}

// Optional number: absent or null is fine, anything else must be a number.
bool ReadOptional(const json& object, const char* key, std::optional<float>& out)
{
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) { out.reset(); return true; }
    if (!it->is_number()) { return false; }
    out = it->get<float>();
    return true;
}

bool ReadBin(const json& object, std::optional<DistBin>& out)
{
    const auto it = object.find("dist_bin");
    if (it == object.end() || it->is_null()) { out.reset(); return true; }
    if (!it->is_string()) { return false; }
    const std::string& text = it->get_ref<const std::string&>();
    if (text == "near") { out = DistBin::Near; }
    else if (text == "mid") { out = DistBin::Mid; }
    else if (text == "far") { out = DistBin::Far; }
    else { return false; }
    return true;
}

std::optional<Event> ReadEvent(const json& object)
{
    if (!object.is_object()) { return std::nullopt; }
    Event event;
    const auto id = object.find("id");
    const auto cls = object.find("cls");
    if (id == object.end() || !id->is_number_integer() || cls == object.end() || !cls->is_string()) { return std::nullopt; }
    event.id = id->get<int>();
    event.cls = cls->get<std::string>();
    if (!IsKnownClass(event.cls)) { return std::nullopt; }
    if (!ReadNumber(object, "conf", event.conf) || !ReadNumber(object, "az", event.az) ||
        !ReadNumber(object, "fb", event.fb) || !ReadNumber(object, "level", event.level)) { return std::nullopt; }
    if (!ReadOptional(object, "dist_m", event.distM) || !ReadOptional(object, "elev_m", event.elevM) ||
        !ReadBin(object, event.distBin)) { return std::nullopt; }
    return event;
}
}

bool IsKnownClass(std::string_view cls)
{
    for (std::string_view known : kClasses)
    {
        if (known == cls) { return true; }
    }
    return false;
}

std::optional<Message> ParseMessage(std::string_view text)
{
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) { return std::nullopt; }
    const auto v = root.find("v");
    const auto src = root.find("src");
    const auto mode = root.find("mode");
    const auto t = root.find("t");
    const auto events = root.find("events");
    if (v == root.end() || !v->is_number_integer() || v->get<int>() != 1) { return std::nullopt; }
    if (src == root.end() || !src->is_string() || mode == root.end() || !mode->is_string()) { return std::nullopt; }
    if (t == root.end() || !t->is_number() || events == root.end() || !events->is_array()) { return std::nullopt; }

    Message message;
    message.src = src->get<std::string>();
    message.mode = mode->get<std::string>();
    message.t = t->get<double>();
    for (const json& item : *events)
    {
        if (auto event = ReadEvent(item)) { message.events.push_back(std::move(*event)); }
    }
    return message;
}
}
