// SPDX-FileCopyrightText: 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/test/unit_test.hpp>

#include <element/settings.hpp>

using element::Settings;

namespace {

struct ColourSettingExpectation
{
    const char* key;
    juce::uint32 fallback;
};

const ColourSettingExpectation colourSettings[] {
    { Settings::popupMenuHighlightedBackgroundColorKey, 0xff4765a0 },
    { Settings::popupMenuHighlightedTextColorKey, 0xffffffff },
    { Settings::audioConnectedPortColorKey, 0xff90ee90 },
    { Settings::audioDisconnectedPortColorKey, 0xff90ee90 },
    { Settings::midiConnectedPortColorKey, 0xffffa500 },
    { Settings::midiDisconnectedPortColorKey, 0xffffa500 },
    { Settings::audioConnectionCableColorKey, 0xff484848 },
    { Settings::midiConnectionCableColorKey, 0xff484848 },
    { Settings::connectionCableDragColorKey, 0xff484848 },
    { Settings::connectionCableHoverColorKey, 0xff666666 },
    { Settings::nearbyConnectHighlightColorKey, 0xff0c83ff },
    { Settings::nearbyDisconnectHighlightColorKey, 0xffff66b3 },
    { Settings::nearbySwapHighlightColorKey, 0xffffffff },
    { Settings::nodeEnabledColorKey, 0xff9ba2ab },
    { Settings::nodeEnabledSelectedColorKey, 0xff4a83c7 },
    { Settings::nodeDisabledColorKey, 0xff454a52 },
    { Settings::nodeDisabledSelectedColorKey, 0xff3d2470 },
};

juce::File makeSettingsDirectory()
{
    auto directory = juce::File::createTempFile ("elementum-settings-colour");
    directory.deleteFile();
    BOOST_REQUIRE (directory.createDirectory());
    return directory;
}

} // namespace

BOOST_AUTO_TEST_SUITE (SettingsColourTests)

BOOST_AUTO_TEST_CASE (InvalidHexColoursUseTheirRegisteredDefaults)
{
    const auto directory = makeSettingsDirectory();

    {
        Settings settings (directory);
        auto* const props = settings.getUserSettings();
        BOOST_REQUIRE (props != nullptr);

        constexpr const char* invalidValues[] {
            "#invalid", "#12345", "#123456789", "#12gg00", "123456", ""
        };

        for (const auto& setting : colourSettings)
            for (const auto* const value : invalidValues)
            {
                props->setValue (setting.key, value);
                BOOST_CHECK_EQUAL (settings.getColour (setting.key).getARGB(), setting.fallback);
            }
    }

    directory.deleteRecursively();
}

BOOST_AUTO_TEST_CASE (SixAndEightDigitHexColoursAreParsed)
{
    const auto directory = makeSettingsDirectory();

    {
        Settings settings (directory);
        auto* const props = settings.getUserSettings();
        BOOST_REQUIRE (props != nullptr);

        props->setValue (Settings::audioConnectionCableColorKey, "#123456");
        BOOST_CHECK_EQUAL (settings.getColour (Settings::audioConnectionCableColorKey).getARGB(), 0xff123456);

        props->setValue (Settings::audioConnectionCableColorKey, "#7fabcdef");
        BOOST_CHECK_EQUAL (settings.getColour (Settings::audioConnectionCableColorKey).getARGB(), 0x7fabcdef);
    }

    directory.deleteRecursively();
}

BOOST_AUTO_TEST_SUITE_END()
