// Copyright 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/test/unit_test.hpp>

#include <element/settings.hpp>

using element::Settings;

namespace {

juce::File makeSettingsDirectory()
{
    auto directory = juce::File::createTempFile ("elementum-settings-plugin");
    directory.deleteFile();
    BOOST_REQUIRE (directory.createDirectory());
    return directory;
}

} // namespace

BOOST_AUTO_TEST_SUITE (SettingsPluginTests)

BOOST_AUTO_TEST_CASE (ManufacturerInitialIndexSettingIsPersisted)
{
    const auto directory = makeSettingsDirectory();

    {
        Settings settings (directory);
        BOOST_CHECK (! settings.groupPluginManufacturersByInitial());
        settings.setGroupPluginManufacturersByInitial (true);
        BOOST_REQUIRE (settings.saveIfNeeded());
    }

    {
        Settings settings (directory);
        BOOST_CHECK (settings.groupPluginManufacturersByInitial());
    }

    directory.deleteRecursively();
}

BOOST_AUTO_TEST_CASE (PluginScanTimeoutSettingIsPersistedAndValidated)
{
    const auto directory = makeSettingsDirectory();

    {
        Settings settings (directory);
        BOOST_CHECK_EQUAL (settings.pluginScanTimeoutSeconds(), 90);
        settings.setPluginScanTimeoutSeconds (300);
        BOOST_REQUIRE (settings.saveIfNeeded());
    }

    {
        Settings settings (directory);
        BOOST_CHECK_EQUAL (settings.pluginScanTimeoutSeconds(), 300);

        auto* const props = settings.getUserSettings();
        BOOST_REQUIRE (props != nullptr);
        props->setValue (Settings::pluginScanTimeoutSecondsKey, 45);
        BOOST_CHECK_EQUAL (settings.pluginScanTimeoutSeconds(), 90);
    }

    directory.deleteRecursively();
}

BOOST_AUTO_TEST_SUITE_END()
