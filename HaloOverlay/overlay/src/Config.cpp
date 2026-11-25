#include "Config.h"

#include <cctype>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace halo
{
namespace
{
void Report(const std::string& text)
{
    OutputDebugStringA(("HaloOverlay config: " + text + "\n").c_str());
}

template <typename T>
void Read(const nlohmann::json& root, const char* key, T& out)
{
    const auto it = root.find(key);
    if (it == root.end()) { return; }
    try { out = it->get<T>(); }
    catch (const nlohmann::json::exception&) { Report(std::string("wrong type for ") + key + ", using the default"); }
}
}

Config LoadConfig(const std::filesystem::path& path)
{
    Config config;
    std::ifstream file(path);
    if (!file) { return config; }
    std::stringstream text;
    text << file.rdbuf();
    const nlohmann::json root = nlohmann::json::parse(text.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        Report("cannot parse " + path.string() + ", using defaults");
        return config;
    }
    Read(root, "port", config.port);
    Read(root, "monitor", config.monitor);
    Read(root, "labels_hotkey", config.labelsHotkey);
    Read(root, "quit_hotkey", config.quitHotkey);
    Read(root, "view_pitch_degrees", config.viewPitchDegrees);
    Read(root, "src", config.src);
    return config;
}

bool ParseHotkey(std::string_view text, UINT& modifiers, UINT& vk)
{
    UINT mods = 0;
    UINT key = 0;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t end = std::min(text.find('+', start), text.size());
        std::string part(text.substr(start, end - start));
        for (char& c : part) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
        if (part == "CTRL") { mods |= MOD_CONTROL; }
        else if (part == "ALT") { mods |= MOD_ALT; }
        else if (part == "SHIFT") { mods |= MOD_SHIFT; }
        else if (part == "WIN") { mods |= MOD_WIN; }
        else if (part.size() == 1 && std::isalnum(static_cast<unsigned char>(part[0]))) { key = static_cast<UINT>(part[0]); }
        else if (part.size() >= 2 && part[0] == 'F' && std::isdigit(static_cast<unsigned char>(part[1])))
        {
            const int n = std::atoi(part.c_str() + 1);
            if (n < 1 || n > 24) { return false; }
            key = VK_F1 + static_cast<UINT>(n - 1);
        }
        else { return false; }
        if (end == text.size()) { break; }
        start = end + 1;
    }
    if (key == 0) { return false; }
    modifiers = mods;
    vk = key;
    return true;
}
}
