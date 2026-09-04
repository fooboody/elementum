// Copyright 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <element/plugindetails.hpp>

namespace element {

inline void showPluginDetails (const juce::PluginDescription& plugin)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                            "Plugin Details",
                                            getPluginDetailsText (plugin));
}

} // namespace element
