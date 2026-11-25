#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Message format v1 between the analyzer / Unreal sender and the overlay (design doc section 6).
namespace halo
{
enum class DistBin { Near, Mid, Far };

struct Event
{
    int id = 0;
    std::string cls;
    float conf = 1.0f;
    float az = 0.0f;   // 0 front, +90 right, +/-180 behind
    float fb = 0.0f;   // front/back certainty; below 1 the azimuth is folded to the front
    float level = 0.0f;
    std::optional<float> distM;
    std::optional<DistBin> distBin;
    std::optional<float> elevM;
};

struct Message
{
    std::string src;
    std::string mode;
    double t = 0.0;
    std::vector<Event> events;
};

/** nullopt when the datagram is not valid JSON, not v1, or its top-level fields are malformed. */
std::optional<Message> ParseMessage(std::string_view json);

bool IsKnownClass(std::string_view cls);
}
