// Copyright 2019-2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <element/context.hpp>
#include <element/services.hpp>
#include <element/settings.hpp>
#include <element/ui/style.hpp>

#include <element/ui/standard.hpp>

#include "ui/nodechannelstrip.hpp"
#include "ui/grapheditorview.hpp"
#include "ui/graphmixerview.hpp"
#include "ui/horizontallistbox.hpp"
#include "ui/viewhelpers.hpp"

#include "common.hpp"

namespace element {

class GraphMixerChannelStrip : public NodeChannelStripComponent,
                               public DragAndDropTarget,
                               public ComponentListener
{
public:
        std::function<void()> onRefreshNeeded;

    std::function<void (GraphMixerChannelStrip&, double)> onFaderValueApplied;
    std::function<void (const Node&, ModifierKeys)> onSelectionRequested;

    std::function<void (const Node&)> onHideRequested;

    GraphMixerChannelStrip (GuiService& gui) : NodeChannelStripComponent (gui, false)
    {
        onNodeChanged = [this]() { setNodeNameEditable (! (getNode().isIONode())); };
        listener.reset (new ChildListener (*this));

        addMouseListener (listener.get(), true);
        onVolumeApplied = [this] (double value) {
            if (onFaderValueApplied)
                onFaderValueApplied (*this, value);
        };

    }

    ~GraphMixerChannelStrip()
    {
        removeMouseListener (listener.get());
        listener.reset();
    }

    void requestSelection (const ModifierKeys& modifiers)
    {
        if (selectionRequested)
            return;

        selectionRequested = true;
        if (onSelectionRequested)
            onSelectionRequested (getNode(), modifiers);
    }

    void mouseDown (const MouseEvent& ev) override
    {
        if (ev.mods.isPopupMenu())
            return;

        if (! down)
        {
            down = true;
            dragging = false;
            selectionRequested = false;
            requestSelection (ev.mods);
        }
    }

    void showContextMenu()
    {
        PopupMenu menu;
        menu.addItem (1, TRANS ("Hide from Mixer"));
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [strip = Component::SafePointer<GraphMixerChannelStrip> (this)] (int result) {
                                if (strip == nullptr || result != 1)
                                    return;
                                if (strip->onHideRequested)
                                    strip->onHideRequested (strip->getNode());
                                else
                                {
                                    strip->getNode().setHiddenInMixer (true);
                                    if (strip->onRefreshNeeded)
                                        strip->onRefreshNeeded();
                                }
                            });
    }

    void mouseDrag (const MouseEvent& ev) override
    {
        if (down && ! dragging)
        {
            dragging = true;
            auto* dnd = findParentComponentOfClass<DragAndDropContainer>();
            Image image (Image::ARGB, 1, 1, true);
            ScaledImage si (image);
            dnd->startDragging (var ("graphMixerStrip"), this, si);
        }
    }

    void mouseMove (const MouseEvent& ev) override
    {
        // noop
    }

    void mouseUp (const MouseEvent& ev) override
    {
        dragging = down = hover = selectionRequested = false;
    }

    bool shouldDrawDragImageWhenOver() override
    {
        return false;
    }

    void itemDragEnter (const SourceDetails&) override
    {
        hover = true;
        repaint();
    }

    void itemDragExit (const SourceDetails&) override
    {
        hover = false;
        repaint();
    }

    void paint (Graphics& g) override
    {
        NodeChannelStripComponent::paint (g);
        if (selected)
        {
            g.setColour (Colours::white);
            g.setOpacity (0.09f);
            g.fillAll();
        }
    }

    void paintOverChildren (Graphics& g) override
    {
        if (selected || (hover && ! dragging && ! down))
        {
            auto borderColor = Colors::toggleBlue;
            if (auto* const cc = ViewHelpers::findContentComponent (this))
                borderColor = cc->context().settings().getColour (Settings::mixerSelectedBorderColorKey);

            g.setColour (borderColor);
            g.drawRect (0.f, 0.f, (float) getWidth(), (float) getHeight(), selected ? 1.5f : 1.0f);
        }

    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return details.description == "graphMixerStrip";
    }

    void itemDropped (const SourceDetails& details) override
    {
        if (details.description == "graphMixerStrip")
        {
            auto* strip = dynamic_cast<GraphMixerChannelStrip*> (details.sourceComponent.get());
            auto myNode = getNode().data();
            auto dNode = strip->getNode().data();
            ValueTree parent = dNode.getParent();

            int myIndex = parent.indexOf (myNode);
            int dIndex = parent.indexOf (dNode);
            if (myIndex >= 0 && dIndex >= 0)
            {
                parent.moveChild (dIndex, myIndex, nullptr);
                if (onRefreshNeeded)
                    onRefreshNeeded();
            }
        }

        hover = false;
        repaint();
    }

    void setSelected (bool isNowSelected)
    {
        if (selected == isNowSelected)
            return;
        selected = isNowSelected;
        repaint();
    }

private:
    bool selected = false;
    bool dragging = false;
    bool down = false;
    bool hover = false;
    bool selectionRequested = false;

    struct ChildListener : public MouseListener
    {
        ChildListener (GraphMixerChannelStrip& o) : owner (o) {}
        void mouseDown (const MouseEvent& ev) override
        {
            if (ev.mods.isPopupMenu())
                owner.showContextMenu();
            else
                owner.requestSelection (ev.mods);
        }

        void mouseUp (const MouseEvent&) override
        {
            owner.selectionRequested = false;
        }

        GraphMixerChannelStrip& owner;
    };

    std::unique_ptr<ChildListener> listener;
};

class GraphMixerListBoxModel : public ListBoxModel
{
public:
        std::function<void()> onSelectionChanged;
    std::function<void (GraphMixerChannelStrip&, double)> onFaderValueApplied;
    std::function<void (const Node&, ModifierKeys)> onSelectionRequested;

    std::function<void (const Node&)> onHideRequested;

    GraphMixerListBoxModel (GuiService& g, HorizontalListBox& b) : gui (g), box (b) { refreshNodes(); }
    ~GraphMixerListBoxModel() {}

    int getNumRows() override
    {
        return nodes.size();
    }

    void paintListBoxItem (int rowNumber, Graphics& g, int width, int height, bool rowIsSelected) override
    {
        ignoreUnused (rowNumber, g, width, height, rowIsSelected);
    }

    Node getNode (int r)
    {
        return nodes[r];
    }

    Component* refreshComponentForRow (int rowNumber, bool isRowSelected, Component* existing) override
    {
        GraphMixerChannelStrip* const strip = existing == nullptr
                                                  ? new GraphMixerChannelStrip (gui)
                                                  : dynamic_cast<GraphMixerChannelStrip*> (existing);
                strip->onRefreshNeeded = std::bind (&GraphMixerListBoxModel::refresh, this);
        strip->onFaderValueApplied = onFaderValueApplied;
        strip->onSelectionRequested = onSelectionRequested;

        strip->onHideRequested = onHideRequested;
        auto node = getNode (rowNumber);
        strip->setNode (node);
        strip->setSelected (isRowSelected);
        return strip;
    }

        void refresh()
    {
        refreshNodes();
        box.updateContent();
    }

    void selectedRowsChanged (int) override
    {
        if (onSelectionChanged)
            onSelectionChanged();
    }

    int getRowForNode (const Node& node) const
    {
        for (int i = 0; i < nodes.size(); ++i)
            if (nodes.getReference (i) == node)
                return i;

        return -1;
    }

    void setNode (const Node& node)
    {
        if (node == _node)
            return;
        _node = node.isGraph() ? node : node.getParentGraph();
        refreshNodes();
    }

    void refreshNodes()
    {
        nodes.clearQuick();
        const auto graph = _node.isGraph() ? _node
                                           : gui.context().session()->getActiveGraph();
        for (int i = 0; i < graph.getNumNodes(); ++i)
        {
            const auto n = graph.getNode (i);
            // clang-format off
            if (n.isMidiIONode() ||
                n.getIdentifier() == EL_NODE_ID_MIDI_INPUT_DEVICE ||
                n.getIdentifier() == EL_NODE_ID_MIDI_OUTPUT_DEVICE ||
                n.isHiddenInMixer())
            {
                continue;
            }
            // clang-format on

            nodes.add (n);
        }
    }

    void backgroundClicked (const MouseEvent& ev) override
    {
        if (! ev.mods.isPopupMenu())
            return;

        const auto graph = _node.isGraph() ? _node
                                           : gui.context().session()->getActiveGraph();
        NodeArray hidden;
        for (int i = 0; i < graph.getNumNodes(); ++i)
        {
            const auto n = graph.getNode (i);
            if (n.isHiddenInMixer())
                hidden.add (n);
        }

        PopupMenu menu;
        for (int i = 0; i < hidden.size(); ++i)
            menu.addItem (i + 2, TRANS ("Show") + " " + hidden.getReference (i).getDisplayName());
        if (hidden.size() > 1)
        {
            menu.addSeparator();
            menu.addItem (1, TRANS ("Show all"));
        }
        else if (hidden.isEmpty())
        {
            menu.addItem (1, TRANS ("No Hidden Channels"), false);
        }

        menu.showMenuAsync (PopupMenu::Options(),
                            [safeBox = Component::SafePointer<Component> (&box), this, hidden] (int result) {
                                if (safeBox == nullptr || result <= 0)
                                    return;
                                if (result == 1)
                                    for (auto n : hidden)
                                        n.setHiddenInMixer (false);
                                else if (result - 2 < hidden.size())
                                    hidden[result - 2].setHiddenInMixer (false);
                                refresh();
                            });
    }

private:
    GuiService& gui;
    HorizontalListBox& box;
    Node _node;
    NodeArray nodes;
    [[maybe_unused]] bool dragging = false;
};

class GraphMixerView::Content : public Component,
                                public DragAndDropContainer,
                                private ValueTree::Listener
{
public:
    Content (GraphMixerView& v, GuiService& gui, Session* sess)
        : ui (gui), session (sess), view (v)
    {
        setOpaque (true);
        addAndMakeVisible (box);
        box.setRowHeight (80);
        box.setMultipleSelectionEnabled (true);
        model.reset (new GraphMixerListBoxModel (gui, box));
                model->onSelectionChanged = [this]() { onMixerSelectionChanged(); };
        model->onFaderValueApplied = [this] (GraphMixerChannelStrip& source, double value) {
            syncSelectedFaderValues (source, value);
        };
        model->onSelectionRequested = [this] (const Node& node, ModifierKeys modifiers) {

            selectMixerNode (node, modifiers);
        };
        model->onHideRequested = [this] (const Node& node) { hideMixerNodes (node); };
        box.setModel (model.get());
        box.updateContent();
        watchNodes (session->getActiveGraph());
        connectGraphEditorSelection();

        _conns.push_back (ui.nodeSelected.connect (std::bind (&Content::onNodeSelected, this)));
        auto& ssrv = *gui.sibling<SessionService>();
        _conns.push_back (ssrv.sigSessionLoaded.connect ([this]() {
            const auto graph = session->getActiveGraph();
            watchNodes (graph);
            model->setNode (graph);
            box.updateContent();
            connectGraphEditorSelection();
            syncMixerSelectionFromGraphEditor();
            repaint();
        }));
    }

    ~Content()
    {
        for (auto& c : _conns)
            c.disconnect();
        _conns.clear();
        graphSelectionConnection.disconnect();
        watchedNodes.removeListener (this);

        box.setModel (nullptr);
        model.reset();
    }

        void onNodeSelected()
    {
        const auto node = ui.getSelectedNode();
        watchNodes (node);
        model->setNode (node);
        box.updateContent();
        connectGraphEditorSelection();
        syncMixerSelectionFromGraphEditor();
    }

    GraphEditorView* getGraphEditorView()
    {
        if (auto* const content = dynamic_cast<StandardContent*> (ViewHelpers::findContentComponent (this)))
            return dynamic_cast<GraphEditorView*> (content->getMainViewComponent());

        return nullptr;
    }

    void connectGraphEditorSelection()
    {
        auto* const editor = getGraphEditorView();
        if (editor == graphEditor)
            return;

        graphSelectionConnection.disconnect();
        graphEditor = editor;
        if (graphEditor != nullptr)
            graphSelectionConnection = graphEditor->editor().getSelectionChangedSignal().connect (
                [this]() { syncMixerSelectionFromGraphEditor(); });
    }

        NodeArray getMixerSelectedNodes() const
    {
        NodeArray nodes;
        for (int i = 0; i < box.getNumSelectedRows(); ++i)
        {
            const int row = box.getSelectedRow (i);
            if (isPositiveAndBelow (row, model->getNumRows()))
                nodes.add (model->getNode (row));
        }

        return nodes;
    }

    static void setDefaultFaderValue (Node node, const double value, const double minimumDecibels)

    {
        if (auto* const object = node.getObject())
        {
            const auto gain = Decibels::decibelsToGain (value, minimumDecibels);
            if (node.isAudioOutputNode())
            {
                node.setProperty (tags::inputGain, gain);
                object->setInputGain (static_cast<float> (gain));
            }
            else
            {
                node.setProperty (tags::gain, gain);
                object->setGain (static_cast<float> (gain));
            }
        }
    }

    void syncSelectedFaderValues (GraphMixerChannelStrip& source, const double value)
    {
        if (syncingFaderValues)
            return;

        const auto nodes = getMixerSelectedNodes();
        if (nodes.size() < 2 || ! nodes.contains (source.getNode()))
            return;

        const juce::ScopedValueSetter<bool> syncing (syncingFaderValues, true);

        const auto minimumDecibels = source.getChannelStrip().getMinDecibels();
        for (const auto& node : nodes)
        {
            if (node == source.getNode())
                continue;

            const auto row = model->getRowForNode (node);
            auto* const strip = row >= 0 ? dynamic_cast<GraphMixerChannelStrip*> (box.getComponentForRowNumber (row)) : nullptr;
            if (strip != nullptr)
                strip->getChannelStrip().setVolume (value, juce::sendNotificationSync);

            else
                setDefaultFaderValue (node, value, minimumDecibels);
        }
    }

    void syncMixerSelectionFromGraphEditor()

    {
        connectGraphEditorSelection();
        if (syncingSelection || graphEditor == nullptr)
            return;

        SparseSet<int> rows;
        for (const auto& node : graphEditor->editor().getSelectedNodes())
        {
            const int row = model->getRowForNode (node);
            if (row >= 0)
                rows.addRange (Range<int> (row, row + 1));
        }

        syncingSelection = true;
        box.setSelectedRows (rows, juce::dontSendNotification);
        syncingSelection = false;
    }

    void onMixerSelectionChanged()
    {
        connectGraphEditorSelection();
        if (syncingSelection || graphEditor == nullptr)
            return;

        const auto nodes = getMixerSelectedNodes();
        syncingSelection = true;
        graphEditor->editor().setSelectedNodes (nodes);
        syncingSelection = false;

        if (nodes.isEmpty())
        {
            if (ui.getSelectedNode().descendsFrom (graphEditor->getGraph()))
                ui.selectNode (Node());
        }
        else if (ui.getSelectedNode() != nodes.getLast())
        {
            ui.selectNode (nodes.getLast());
        }
    }

    void selectMixerNode (const Node& node, ModifierKeys modifiers)
    {
        const int row = model->getRowForNode (node);
        if (row >= 0)
            box.selectRowsBasedOnModifierKeys (row, modifiers, false);
    }

    void hideMixerNodes (const Node& node)
    {
        auto nodes = getMixerSelectedNodes();
        if (! nodes.contains (node))
        {
            nodes.clearQuick();
            nodes.add (node);
        }

        for (auto& selectedNode : nodes)
            selectedNode.setHiddenInMixer (true);

        model->refresh();
        syncMixerSelectionFromGraphEditor();
    }

    void watchNodes (const Node& node)
    {
        const auto graph = node.isGraph() ? node : node.getParentGraph();
        const auto nodes = graph.getNodesValueTree();
        if (nodes == watchedNodes)
            return;

        watchedNodes.removeListener (this);
        watchedNodes = nodes;
        watchedNodes.addListener (this);
    }

    void valueTreeChildAdded (ValueTree& parent, ValueTree& child) override
    {
        if (parent == watchedNodes && child.hasType (types::Node))
        {
            model->refresh();
            repaint();
        }
    }

    void valueTreeChildRemoved (ValueTree& parent, ValueTree& child, int) override
    {
        if (parent == watchedNodes && child.hasType (types::Node))
        {
            model->refresh();
            repaint();
        }
    }

    void valueTreeChildOrderChanged (ValueTree& parent, int, int) override
    {
        if (parent == watchedNodes)
            model->refresh();
    }

    void valueTreePropertyChanged (ValueTree&, const Identifier&) override {}
    void valueTreeParentChanged (ValueTree&) override {}
    void valueTreeRedirected (ValueTree&) override {}

    void resized() override
    {
        box.setBounds (getLocalBounds());
    }

    void paint (Graphics& g) override
    {
        auto backgroundColor = Colors::contentBackgroundColor;
        if (auto* const cc = ViewHelpers::findContentComponent (this))
            backgroundColor = cc->context().settings().getColour (Settings::mixerBackgroundColorKey);

        g.fillAll (backgroundColor);

        if (model->getNumRows() <= 0)
        {
            g.setColour (Colors::textColor);
            g.setFont (Font (FontOptions (15.f)));
            g.drawText (TRANS ("No channels to display"),
                        getLocalBounds().toFloat(),
                        Justification::centred);
        }
    }

    void stabilize()
    {
        model->refreshNodes();
        box.updateContent();
        connectGraphEditorSelection();
        syncMixerSelectionFromGraphEditor();
        repaint();
    }

private:
    UI& ui;
    SessionPtr session;
    [[maybe_unused]] GraphMixerView& view;
    std::unique_ptr<GraphMixerListBoxModel> model;
    HorizontalListBox box;
    ValueTree watchedNodes;
    GraphEditorView* graphEditor = nullptr;
        SignalConnection graphSelectionConnection;
    bool syncingSelection = false;
    bool syncingFaderValues = false;

    std::vector<SignalConnection> _conns;
};

GraphMixerView::GraphMixerView()
{
    setName (EL_VIEW_GRAPH_MIXER);
}

GraphMixerView::~GraphMixerView()
{
    content = nullptr;
}

void GraphMixerView::resized()
{
    if (content)
        content->setBounds (getLocalBounds());
}

void GraphMixerView::stabilizeContent()
{
    if (content)
        content->stabilize();
}

void GraphMixerView::initializeView (Services& app)
{
    content.reset (new Content (*this, *app.find<GuiService>(), app.context().session()));
    addAndMakeVisible (content.get());
    content->stabilize();
}

} // namespace element
