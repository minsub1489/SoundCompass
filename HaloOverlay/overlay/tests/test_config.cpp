#include "Check.h"
#include "Config.h"

#include <fstream>

using namespace halo;

static std::filesystem::path Temp(const char* name, const char* content)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path) << content;
    return path;
}

static void missing_file_defaults()
{
    const Config c = LoadConfig(std::filesystem::temp_directory_path() / "halo_no_such_config.json");
    CHECK(c.port == 47800);
    CHECK(c.monitor == 0);
    CHECK(c.viewPitchDegrees == 30.0f);
    CHECK(c.src == "model");
    CHECK(c.labelsHotkey == "Ctrl+Alt+L" && c.quitHotkey == "Ctrl+Alt+Q");
}

static void broken_file_defaults()
{
    const Config c = LoadConfig(Temp("halo_broken_config.json", "{"));
    CHECK(c.port == 47800 && c.monitor == 0);
}

static void partial_file_merges()
{
    const Config c = LoadConfig(Temp("halo_partial_config.json", R"({"port": 47900, "src": "ue"})"));
    CHECK(c.port == 47900);
    CHECK(c.src == "ue");
    CHECK(c.monitor == 0 && c.viewPitchDegrees == 30.0f);
}

static void wrong_type_keeps_default()
{
    const Config c = LoadConfig(Temp("halo_typed_config.json", R"({"port": "x", "monitor": 2})"));
    CHECK(c.port == 47800);
    CHECK(c.monitor == 2);
}

static void hotkey_parse()
{
    UINT mods = 0, vk = 0;
    CHECK(ParseHotkey("Ctrl+Alt+L", mods, vk));
    CHECK(mods == (MOD_CONTROL | MOD_ALT) && vk == 'L');
    CHECK(ParseHotkey("Ctrl+Shift+F9", mods, vk));
    CHECK(mods == (MOD_CONTROL | MOD_SHIFT) && vk == VK_F9);
    CHECK(!ParseHotkey("Banana", mods, vk));
    CHECK(!ParseHotkey("Ctrl+", mods, vk));
}

static void hotkey_registration_reports_conflicts()
{
    CHECK(RegisterHotkeyText(nullptr, 101, "Ctrl+Alt+Shift+F23"));
    CHECK(!RegisterHotkeyText(nullptr, 102, "Ctrl+Alt+Shift+F23")); // taken: the caller must be told
    CHECK(!RegisterHotkeyText(nullptr, 103, "Banana"));
    UnregisterHotKey(nullptr, 101);
}

int main()
{
    RUN(missing_file_defaults);
    RUN(broken_file_defaults);
    RUN(partial_file_merges);
    RUN(wrong_type_keeps_default);
    RUN(hotkey_parse);
    RUN(hotkey_registration_reports_conflicts);
    return g_failures;
}
