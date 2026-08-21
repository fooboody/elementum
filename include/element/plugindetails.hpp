// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <element/juce/audio_processors.hpp>

namespace element {

inline juce::String pluginDetailValue (const juce::String& value)
{
    return value.isNotEmpty() ? value : juce::String ("Not Available");
}

inline juce::String getPluginDetailsText (const juce::PluginDescription& plugin)
{
    juce::StringArray lines;
    lines.add (pluginDetailValue (plugin.name));
    lines.add (juce::String());
    lines.add ("Manufacturer: " + pluginDetailValue (plugin.manufacturerName));
    lines.add ("Version: " + pluginDetailValue (plugin.version));
    lines.add ("Format: " + pluginDetailValue (plugin.pluginFormatName));
    lines.add ("Category: " + pluginDetailValue (plugin.category));
    lines.add ("Type: " + juce::String (plugin.isInstrument ? "Instrument" : "Effect"));
    lines.add ("Audio I/O: " + juce::String (plugin.numInputChannels) + " In / "
               + juce::String (plugin.numOutputChannels) + " Out");

    if (plugin.descriptiveName.isNotEmpty() && plugin.descriptiveName != plugin.name)
    {
        lines.add (juce::String());
        lines.add ("Description:");
        lines.add (plugin.descriptiveName);
    }

    lines.add (juce::String());
    lines.add ("Location:");
    lines.add (pluginDetailValue (plugin.fileOrIdentifier));
    lines.add (juce::String());
    lines.add ("Identifier: " + plugin.createIdentifierString());
    lines.add ("Unique ID: " + juce::String (plugin.uniqueId));
    lines.add ("ARA Extension: " + juce::String (plugin.hasARAExtension ? "Yes" : "No"));
    lines.add ("Shared Container: " + juce::String (plugin.hasSharedContainer ? "Yes" : "No"));
    return lines.joinIntoString ("\n");
}

inline juce::var getPluginDetailsJson (const juce::PluginDescription& plugin)
{
    auto details = std::make_unique<juce::DynamicObject>();
    details->setProperty ("name", plugin.name);
    details->setProperty ("manufacturer", plugin.manufacturerName);
    details->setProperty ("version", plugin.version);
    details->setProperty ("format", plugin.pluginFormatName);
    details->setProperty ("category", plugin.category);
    details->setProperty ("isInstrument", plugin.isInstrument);
    details->setProperty ("audioInputs", plugin.numInputChannels);
    details->setProperty ("audioOutputs", plugin.numOutputChannels);

    if (plugin.descriptiveName.isNotEmpty() && plugin.descriptiveName != plugin.name)
        details->setProperty ("description", plugin.descriptiveName);

    details->setProperty ("location", plugin.fileOrIdentifier);
    details->setProperty ("identifier", plugin.createIdentifierString());
    details->setProperty ("uniqueId", plugin.uniqueId);
    details->setProperty ("hasARAExtension", plugin.hasARAExtension);
    details->setProperty ("hasSharedContainer", plugin.hasSharedContainer);
    return juce::var (details.release());
}

inline juce::String getPluginDetailsJsonText (const juce::PluginDescription& plugin)
{
    return juce::JSON::toString (getPluginDetailsJson (plugin), true);
}

} // namespace element
