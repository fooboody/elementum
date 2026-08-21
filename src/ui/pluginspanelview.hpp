// Copyright 2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EL_PLUGINS_PANEL_VIEW_H
#define EL_PLUGINS_PANEL_VIEW_H

#include <element/ui/content.hpp>

namespace element {

class PluginManager;
class PluginsPanelView;

class PluginSearchTextEditor : public TextEditor
{
public:
    explicit PluginSearchTextEditor (PluginsPanelView&);

    void addPopupMenuItems (PopupMenu&, const MouseEvent*) override;
    void performPopupMenuAction (int) override;

private:
    PluginsPanelView& owner;
    static constexpr int includeManufacturersMenuItem = 0x5001;
};

class PluginsPanelView : public ContentView,

                         public ChangeListener,
                         public TextEditor::Listener,
                         public Timer
{
public:
    PluginsPanelView (PluginManager& pm);
    ~PluginsPanelView();

    void resized() override;
    void paint (Graphics&) override;

    /** Returns the text in the search box */
    String getSearchText() const { return search.getText().trim(); }
    bool isFiltering() const { return getSearchText().isNotEmpty(); }

    /** Opens or closes every category currently shown in the tree. */
    void setAllCategoriesOpen (bool shouldOpen);

    bool includeManufacturersInSearch();
    void setIncludeManufacturersInSearch (bool shouldInclude);

    /** @internal */
    void textEditorTextChanged (TextEditor&) override;
    void textEditorReturnKeyPressed (TextEditor&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

private:
    PluginManager& plugins;
    TreeView tree;
    PluginSearchTextEditor search;

    void updateTreeView();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginsPanelView);
};

} // namespace element

#endif // EL_PLUGINS_PANEL_VIEW_H
