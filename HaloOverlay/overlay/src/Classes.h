#pragma once

#include <string_view>

namespace halo
{
/**
 * Look per sound class. Colours carried over from SoundHaloWidget.cpp CategoryColor (Voice = Conversation,
 * Alarm = Alert, Throwable = Throw); new classes reuse the in-game noise hues, none in the red family,
 * which stays Gunshot's. Importance follows DefaultGame.ini CategoryImportance.
 */
struct ClassStyle
{
    float r, g, b;
    const wchar_t* icon;    // file stem in icons/, nullptr = procedural wave, L"!" = drawn exclamation mark
    float baseImportance;
};

inline const ClassStyle& StyleOf(std::string_view cls)
{
    struct Entry { std::string_view name; ClassStyle style; };
    static const Entry table[] = {
        {"Gunshot", {1.00f, 0.25f, 0.20f, L"Bullet", 0.95f}},
        {"Explosion", {1.00f, 0.64f, 0.12f, L"Grenade", 1.00f}},
        {"Footstep", {0.25f, 0.90f, 0.54f, L"Footprint", 0.75f}},
        {"Door", {0.35f, 0.65f, 1.00f, L"Door", 0.55f}},
        {"Voice", {0.80f, 0.50f, 1.00f, nullptr, 0.70f}},
        {"Alarm", {1.00f, 0.92f, 0.20f, L"!", 0.90f}},
        {"GlassBreak", {0.90f, 0.95f, 1.00f, L"GlassBreak", 0.80f}},
        {"Throwable", {0.72f, 0.52f, 0.32f, L"Throw", 0.60f}},
        {"Reload", {0.60f, 0.55f, 1.00f, nullptr, 0.85f}},
        {"Vehicle", {0.70f, 0.95f, 0.25f, L"Wheel", 0.50f}},
        {"Dog", {1.00f, 0.75f, 0.55f, nullptr, 0.50f}},
        {"Fire", {1.00f, 0.45f, 0.78f, nullptr, 0.45f}},
        {"Impact", {0.70f, 0.75f, 0.80f, nullptr, 0.45f}},
        {"Item", {0.40f, 1.00f, 0.85f, nullptr, 0.40f}},
    };
    static const ClassStyle other{0.25f, 0.83f, 0.90f, nullptr, 0.35f};
    for (const Entry& entry : table)
    {
        if (entry.name == cls) { return entry.style; }
    }
    return other;
}
}
