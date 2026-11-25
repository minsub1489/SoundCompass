#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <windows.h>

namespace halo
{
struct Config
{
    int port = 47800;
    int monitor = 0;
    std::string labelsHotkey = "Ctrl+Alt+L";
    std::string quitHotkey = "Ctrl+Alt+Q";
    float viewPitchDegrees = 30.0f;
    std::string src = "model";
};

/** Defaults for a missing file; a broken file or a wrongly typed key also falls back (reported via OutputDebugString). */
Config LoadConfig(const std::filesystem::path& path);

/** "Ctrl+Alt+L" -> MOD_CONTROL | MOD_ALT, 'L'. Accepts Ctrl, Alt, Shift, Win, A-Z, 0-9, F1-F24. */
bool ParseHotkey(std::string_view text, UINT& modifiers, UINT& vk);
}
