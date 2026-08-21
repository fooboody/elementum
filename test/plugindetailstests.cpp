// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/test/unit_test.hpp>

#include <element/plugindetails.hpp>

using namespace juce;
using namespace element;

BOOST_AUTO_TEST_SUITE (PluginDetailsTests)

BOOST_AUTO_TEST_CASE (TextMatchesDialogFormat)
{
    PluginDescription plugin;
    plugin.name = "Example Synth";
    plugin.manufacturerName = "Example Audio";
    plugin.version = "1.2.3";
    plugin.pluginFormatName = "VST3";
    plugin.category = "Instrument";
    plugin.isInstrument = true;
    plugin.numInputChannels = 2;
    plugin.numOutputChannels = 4;
    plugin.fileOrIdentifier = "C:/Plugins/Example Synth.vst3";
    plugin.uniqueId = 42;
    plugin.hasARAExtension = true;
    plugin.hasSharedContainer = false;

    const auto text = getPluginDetailsText (plugin);
    BOOST_CHECK (text.contains ("Type: Instrument"));
    BOOST_CHECK (text.contains ("Audio I/O: 2 In / 4 Out"));
    BOOST_CHECK (text.contains ("ARA Extension: Yes"));
    BOOST_CHECK (text.contains ("Shared Container: No"));
    BOOST_CHECK (! text.contains ("Description:"));
}

BOOST_AUTO_TEST_CASE (JsonPreservesBooleanPluginFlags)
{
    PluginDescription plugin;
    plugin.name = "Example Effect";
    plugin.manufacturerName = "Example Audio";
    plugin.pluginFormatName = "CLAP";
    plugin.isInstrument = false;
    plugin.numInputChannels = 2;
    plugin.numOutputChannels = 2;
    plugin.descriptiveName = "A short description.";
    plugin.fileOrIdentifier = "C:/Plugins/Example Effect.clap";
    plugin.uniqueId = 77;
    plugin.hasARAExtension = false;
    plugin.hasSharedContainer = true;

    const auto json = JSON::parse (getPluginDetailsJsonText (plugin));
    auto* object = json.getDynamicObject();
    BOOST_REQUIRE (object != nullptr);
    BOOST_CHECK_EQUAL (object->getProperty ("name").toString(), "Example Effect");
    BOOST_CHECK (! static_cast<bool> (object->getProperty ("isInstrument")));
    BOOST_CHECK (! static_cast<bool> (object->getProperty ("hasARAExtension")));
    BOOST_CHECK (static_cast<bool> (object->getProperty ("hasSharedContainer")));
    BOOST_CHECK_EQUAL ((int) object->getProperty ("audioInputs"), 2);
    BOOST_CHECK_EQUAL ((int) object->getProperty ("audioOutputs"), 2);
    BOOST_CHECK_EQUAL (object->getProperty ("description").toString(), "A short description.");
}

BOOST_AUTO_TEST_SUITE_END()
