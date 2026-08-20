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

BOOST_AUTO_TEST_SUITE_END()
