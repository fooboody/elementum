// Copyright 2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <element/plugins.hpp>
#include "ui/guicommon.hpp"
#include "ui/pluginspanelview.hpp"

namespace element {
namespace {

bool pluginMatchesSearch (const PluginDescription& plugin, const String& searchText)
{
    return searchText.isEmpty() || plugin.name.containsIgnoreCase (searchText);
}

bool folderContainsSearchMatch (const KnownPluginList::PluginTree& folder,
                                const String& searchText)
{
    for (const auto& plugin : folder.plugins)
        if (pluginMatchesSearch (plugin, searchText))
            return true;

    for (const auto* subFolder : folder.subFolders)
        if (folderContainsSearchMatch (*subFolder, searchText))
            return true;

    return false;
}

bool shouldIncludeFolder (const PluginsPanelView& panel,
                          const KnownPluginList::PluginTree& folder)
{
    return ! panel.isFiltering()
        || folderContainsSearchMatch (folder, panel.getSearchText());
}

void setCategoryOpennessRecursively (TreeViewItem& parent, const bool shouldOpen)
{
    for (int i = 0; i < parent.getNumSubItems(); ++i)
    {
        auto* const child = parent.getSubItem (i);
        if (! child->mightContainSubItems())
            continue;

        child->setOpenness (shouldOpen ? TreeViewItem::Openness::opennessOpen
                                       : TreeViewItem::Openness::opennessClosed);

        if (shouldOpen)
            setCategoryOpennessRecursively (*child, true);
    }
}

} // namespace

class PluginTreeViewItem : public TreeViewItem
{
public:
    PluginTreeViewItem (const PluginDescription& d)
        : desc (new PluginDescription (d)) {}
    bool mightContainSubItems() override { return false; }
    const std::unique_ptr<const PluginDescription> desc;

    var getDragSourceDescription() override
    {
        var result;
        result.append ("plugin");
        result.append (desc->createIdentifierString());
        return result;
    }

    static String shortFormatName (const String& name)
    {
        if (name == "VST")
            return "vst";
        else if (name == "AudioUnit")
            return "au";
        else if (name == "VST3")
            return "vst3";
        return String();
    }
    void paintItem (Graphics& g, int width, int height) override
    {
        g.setColour (element::Colors::textColor.darker (0.22f));
        String text = desc->name;
        String extra = shortFormatName (desc->pluginFormatName);

        const int leftSide = (width * 4) / 5;
        g.drawText (text, 0, 0, leftSide, height, Justification::centredLeft);
        if (extra.isNotEmpty())
        {
            g.setColour (element::Colors::textColor.withAlpha (0.8f));
            extra = String ("(") + extra + String (")");
            g.setFont (Font (FontOptions (12.f)));
            g.drawText (extra, leftSide, 0, width - leftSide - 3, height, Justification::centredRight);
        }
    }
};

class PluginFolderTreeViewItem : public TreeViewItem
{
public:
    enum MenuResult
    {
        expandAll = 1,
        collapseAll
    };

    PluginFolderTreeViewItem (PluginsPanelView& o, KnownPluginList::PluginTree& t)
        : tree (t), panel (o)
    {
    }

    bool mightContainSubItems() override { return true; }
    KnownPluginList::PluginTree& tree;
    PluginsPanelView& panel;
    void paintItem (Graphics& g, int width, int height) override
    {
        g.setColour (element::Colors::textColor);
        g.drawText (tree.folder, 6, 0, width - 6, height, Justification::centredLeft);
    }

    void itemClicked (const MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            return;

        PopupMenu menu;
        menu.addItem (expandAll, TRANS ("Expand All"));
        menu.addItem (collapseAll, TRANS ("Collapse All"));

        switch (menu.show())
        {
            case expandAll: panel.setAllCategoriesOpen (true); break;
            case collapseAll: panel.setAllCategoriesOpen (false); break;
            default: break;
        }
    }

    void itemOpennessChanged (bool isNowOpen) override
    {
        clearSubItems();
        if (! isNowOpen)
            return;

        const auto text = panel.getSearchText();
        for (auto* folder : tree.subFolders)
            if (shouldIncludeFolder (panel, *folder))
                addSubItem (new PluginFolderTreeViewItem (panel, *folder));

        for (const auto& plugin : tree.plugins)
            if (pluginMatchesSearch (plugin, text))
                addSubItem (new PluginTreeViewItem (plugin));
    }
};

class PluginsPanelTreeRootItem : public TreeViewItem
{
public:
    PluginsPanelTreeRootItem (PluginsPanelView& o, PluginManager& p)
        : owner (o),
          plugins (p)
    {
        data = KnownPluginList::createTree (p.getVisiblePluginTypes(),
                                            KnownPluginList::sortByCategory);
    }

    bool mightContainSubItems() override { return true; }

    void itemOpennessChanged (bool isNowOpen) override
    {
        clearSubItems();
        if (! isNowOpen)
            return;

        for (auto* folder : data->subFolders)
            if (shouldIncludeFolder (owner, *folder))
                addSubItem (new PluginFolderTreeViewItem (owner, *folder));
    }

    PluginsPanelView& owner;
    PluginManager& plugins;

    std::unique_ptr<KnownPluginList::PluginTree> data;
};

PluginsPanelView::PluginsPanelView (PluginManager& p)
    : plugins (p)
{
    addAndMakeVisible (search);
    search.setTextToShowWhenEmpty (TRANS ("Search..."), Colors::textColor.darker());
    search.addListener (this);

    addAndMakeVisible (tree);
    tree.setRootItemVisible (false);
    tree.setOpenCloseButtonsVisible (true);
    tree.setIndentSize (10);
    plugins.getKnownPlugins().addChangeListener (this);
    updateTreeView();
}

PluginsPanelView::~PluginsPanelView()
{
    plugins.getKnownPlugins().removeChangeListener (this);
    tree.getRootItem()->clearSubItems();
    tree.deleteRootItem();
}

void PluginsPanelView::resized()
{
    auto r (getLocalBounds().reduced (2));
    search.setBounds (r.removeFromTop (22));
    r.removeFromTop (2);
    tree.setBounds (r);
}

void PluginsPanelView::paint (Graphics& g) {}

void PluginsPanelView::textEditorTextChanged (TextEditor&)
{
    startTimer (200);
}

void PluginsPanelView::updateTreeView()
{
    tree.deleteRootItem();
    tree.setRootItem (new PluginsPanelTreeRootItem (*this, plugins));

    auto* const root = tree.getRootItem();
    root->setOpenness (TreeViewItem::Openness::opennessOpen);

    if (isFiltering())
        setAllCategoriesOpen (true);
}

void PluginsPanelView::setAllCategoriesOpen (const bool shouldOpen)
{
    auto* const root = tree.getRootItem();
    if (root == nullptr)
        return;

    root->setOpenness (TreeViewItem::Openness::opennessOpen);
    setCategoryOpennessRecursively (*root, shouldOpen);
    tree.repaint();
}

void PluginsPanelView::timerCallback()
{
    updateTreeView();
    stopTimer();
}

void PluginsPanelView::textEditorReturnKeyPressed (TextEditor&)
{
    stopTimer();
    updateTreeView();
}

void PluginsPanelView::changeListenerCallback (ChangeBroadcaster*)
{
    stopTimer();
    updateTreeView();
}

} // namespace element
