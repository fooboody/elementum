// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <element/juce.hpp>

namespace element {

inline String pluginDetailValue (const String& value)
{
    return value.isNotEmpty() ? value : String ("Not Available");
}

inline String getPluginDetailsText (const PluginDescription& plugin)
{
    StringArray lines;
    lines.add (pluginDetailValue (plugin.name));
    lines.add (String());
    lines.add ("Manufacturer: " + pluginDetailValue (plugin.manufacturerName));
    lines.add ("Version: " + pluginDetailValue (plugin.version));
    lines.add ("Format: " + pluginDetailValue (plugin.pluginFormatName));
    lines.add ("Category: " + pluginDetailValue (plugin.category));
    lines.add ("Type: " + String (plugin.isInstrument ? "Instrument" : "Effect"));
    lines.add ("Audio I/O: " + String (plugin.numInputChannels) + " In / "
               + String (plugin.numOutputChannels) + " Out");

    if (plugin.descriptiveName.isNotEmpty() && plugin.descriptiveName != plugin.name)
    {
        lines.add (String());
        lines.add ("Description:");
        lines.add (plugin.descriptiveName);
    }

    lines.add (String());
    lines.add ("Location:");
    lines.add (pluginDetailValue (plugin.fileOrIdentifier));
    lines.add (String());
    lines.add ("Identifier: " + plugin.createIdentifierString());
    lines.add ("Unique ID: " + String (plugin.uniqueId));
    lines.add ("ARA Extension: " + String (plugin.hasARAExtension ? "Yes" : "No"));
    lines.add ("Shared Container: " + String (plugin.hasSharedContainer ? "Yes" : "No"));
    return lines.joinIntoString ("\n");
}

inline void showPluginDetails (const PluginDescription& plugin)
{
    AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon,
                                      "Plugin Details",
                                      getPluginDetailsText (plugin));
}

} // namespace element
