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
    { Settings::audioConnectedPortColorKey, 0xff02de23 },
    { Settings::audioDisconnectedPortColorKey, 0xff2b9e05 },
    { Settings::controlConnectedPortColorKey, 0xffadd8e6 },
    { Settings::controlDisconnectedPortColorKey, 0xff7e9ea8 },
    { Settings::otherConnectedPortColorKey, 0xffff0000 },
    { Settings::otherDisconnectedPortColorKey, 0xffff0000 },
    { Settings::midiConnectedPortColorKey, 0xffffa200 },
    { Settings::midiDisconnectedPortColorKey, 0xffc26c02 },
    { Settings::audioConnectionCableColorKey, 0xff484848 },
    { Settings::midiConnectionCableColorKey, 0xff484848 },
    { Settings::connectionCableDragColorKey, 0xffdddddd },
    { Settings::connectionCableHoverColorKey, 0xffdddddd },
    { Settings::nearbyConnectHighlightColorKey, 0xffffffff },
    { Settings::nearbyDisconnectHighlightColorKey, 0xffff0000 },
    { Settings::nearbySwapHighlightColorKey, 0xffffffff },
    { Settings::nodeEnabledColorKey, 0xff4b65a4 },
    { Settings::nodeEnabledSelectedColorKey, 0xff7eb9fb },
    { Settings::nodeDisabledColorKey, 0xff454a52 },
    { Settings::nodeDisabledSelectedColorKey, 0xff707380 },
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

BOOST_AUTO_TEST_CASE (ThreeDigitHexColoursAreExpandedAndPersisted)
{
    const auto directory = makeSettingsDirectory();

    {
        Settings settings (directory);
        auto* const props = settings.getUserSettings();
        BOOST_REQUIRE (props != nullptr);
        props->setValue (Settings::audioConnectionCableColorKey, "#abc");
        BOOST_REQUIRE (props->saveIfNeeded());
    }

    {
        Settings settings (directory);
        auto* const props = settings.getUserSettings();
        BOOST_REQUIRE (props != nullptr);
        BOOST_CHECK_EQUAL (settings.getColour (Settings::audioConnectionCableColorKey).getARGB(), 0xffaabbcc);
        BOOST_CHECK_EQUAL (props->getValue (Settings::audioConnectionCableColorKey), "#aabbcc");
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
