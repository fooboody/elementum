// SPDX-FileCopyrightText: Copyright (C) Kushview, LLC.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <boost/test/unit_test.hpp>

#include <element/context.hpp>
#include <element/plugins.hpp>
#include <element/settings.hpp>

#include "engine/clapprovider.hpp"
#include "utils.hpp"

using namespace element;

namespace {

/** Returns true if the unverified list contains the given path for a format. */
static bool unverifiedContains (PluginManager& manager, const juce::String& format, const juce::String& path)
{
    juce::OwnedArray<juce::PluginDescription> plugs;
    manager.getUnverifiedPlugins (format, plugs);
    for (const auto* const d : plugs)
        if (d->fileOrIdentifier == path)
            return true;
    return false;
}

} // namespace

BOOST_AUTO_TEST_SUITE (PluginManagerTests)

BOOST_AUTO_TEST_CASE (SupportedFormats)
{
    PluginManager manager;
    for (const auto& supported : Util::compiledAudioPluginFormats())
        BOOST_REQUIRE (! manager.isAudioPluginFormatSupported (supported));

    manager.getNodeFactory().add (new CLAPProvider());
    manager.addDefaultFormats();
    for (const auto& supported : Util::compiledAudioPluginFormats())
        BOOST_REQUIRE_MESSAGE (manager.isAudioPluginFormatSupported (supported), supported.toStdString());
}

#if ! JUCE_MAC
BOOST_AUTO_TEST_CASE (UnverifiedClapPlugins)
{
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("element-unverified-claps");
    tempDir.deleteRecursively();
    BOOST_REQUIRE (tempDir.createDirectory().wasOk());
    const auto fakeClap = tempDir.getChildFile ("Fake.clap");
    BOOST_REQUIRE (fakeClap.create().wasOk());

    const auto propsFile = tempDir.getChildFile ("test.properties");
    juce::PropertiesFile::Options opts;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile props (propsFile, opts);
    props.setValue (juce::String (Settings::lastPluginScanPathPrefix) + "CLAP",
                    tempDir.getFullPathName());

    PluginManager manager;
    manager.getNodeFactory().add (new CLAPProvider());
    manager.addDefaultFormats();
    manager.setPropertiesFile (&props);
    manager.searchUnverifiedPlugins();

    bool found = false;
    for (int retry = 0; ! found && retry < 250; ++retry)
    {
                found = unverifiedContains (manager, "CLAP", fakeClap.getFullPathName());

        if (! found)
            juce::Thread::sleep (20);
    }
    BOOST_REQUIRE_MESSAGE (found, "unverified scan should find the CLAP file");

    // Once known with an "id:path" identifier, it is no longer unverified.
    juce::PluginDescription desc;
    desc.pluginFormatName = "CLAP";
    desc.name = "Fake";
    desc.fileOrIdentifier = "com.fake.id:" + fakeClap.getFullPathName();
    manager.getKnownPlugins().addType (desc);
    BOOST_REQUIRE (! unverifiedContains (manager, "CLAP", fakeClap.getFullPathName()));

    manager.setPropertiesFile (nullptr);
    props.setNeedsToBeSaved (false);
    tempDir.deleteRecursively();
}
#endif

#if JUCE_WINDOWS && JUCE_PLUGINHOST_VST3
BOOST_AUTO_TEST_CASE (UnverifiedVST3BundlePaths)
{
    const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("element-unverified-vst3");
    tempDir.deleteRecursively();
    BOOST_REQUIRE (tempDir.createDirectory().wasOk());

    const auto bundle = tempDir.getChildFile ("Bundled.vst3");
    BOOST_REQUIRE (bundle.createDirectory().wasOk());
    const auto bundledBinary = bundle.getChildFile ("Contents")
                                  .getChildFile ("x86_64-win")
                                  .getChildFile ("Bundled.vst3");
    BOOST_REQUIRE (bundledBinary.getParentDirectory().createDirectory().wasOk());
    BOOST_REQUIRE (bundledBinary.create().wasOk());

    const auto directBinary = tempDir.getChildFile ("Direct.vst3");
    BOOST_REQUIRE (directBinary.create().wasOk());
    const auto unknownBundle = tempDir.getChildFile ("Unknown.vst3");
    BOOST_REQUIRE (unknownBundle.createDirectory().wasOk());

    const auto propsFile = tempDir.getChildFile ("test.properties");
    juce::PropertiesFile::Options opts;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    juce::PropertiesFile props (propsFile, opts);
    props.setValue (juce::String (Settings::lastPluginScanPathPrefix) + "VST3",
                    tempDir.getFullPathName());

    PluginManager manager;
    manager.addDefaultFormats();
    manager.setPropertiesFile (&props);

    juce::PluginDescription bundledDescription;
    bundledDescription.pluginFormatName = "VST3";
    bundledDescription.name = "Bundled";
    bundledDescription.fileOrIdentifier = bundledBinary.getFullPathName();
    manager.getKnownPlugins().addType (bundledDescription);

    juce::PluginDescription directDescription;
    directDescription.pluginFormatName = "VST3";
    directDescription.name = "Direct";
    directDescription.fileOrIdentifier = directBinary.getFullPathName();
    manager.getKnownPlugins().addType (directDescription);

    manager.searchUnverifiedPlugins();

    bool foundUnknown = false;
    for (int retry = 0; ! foundUnknown && retry < 250; ++retry)
    {
        foundUnknown = unverifiedContains (manager, "VST3", unknownBundle.getFullPathName());
        if (! foundUnknown)
            juce::Thread::sleep (20);
    }

    BOOST_REQUIRE_MESSAGE (foundUnknown, "unverified scan should find the unknown VST3 bundle");
    BOOST_REQUIRE (! unverifiedContains (manager, "VST3", bundle.getFullPathName()));
    BOOST_REQUIRE (! unverifiedContains (manager, "VST3", directBinary.getFullPathName()));

    manager.setPropertiesFile (nullptr);
    props.setNeedsToBeSaved (false);
    tempDir.deleteRecursively();
}
#endif

BOOST_AUTO_TEST_SUITE_END()
