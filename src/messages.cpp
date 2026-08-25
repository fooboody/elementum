// Copyright 2023 Kushview, LLC <info@kushview.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <element/context.hpp>
#include <element/engine.hpp>
#include <element/plugins.hpp>
#include <element/signals.hpp>
#include <element/settings.hpp>
#include <element/session.hpp>

#include "messages.hpp"

using namespace juce;

namespace element {

class AddPluginAction : public juce::UndoableAction
{
public:
    AddPluginAction (Services& _app, const AddPluginMessage& msg)
        : app (_app), graph (msg.graph), description (msg.description), builder (msg.builder), verified (msg.verified) {}
    ~AddPluginAction() noexcept {}

    bool perform() override
    {
        addedNode = Node();
        if (auto* ec = app.find<EngineService>())
            if (graph.isGraph())
                addedNode = addPlugin (*ec);
        return addedNode.isValid();
    }

    bool undo() override
    {
        if (! addedNode.isValid())
            return false;
        if (! havePosition)
            addedNode.getRelativePosition (x, y);
        havePosition = true;
        if (auto* ec = app.find<EngineService>())
            ec->removeNode (addedNode);
        addedNode = Node();
        return true;
    }

private:
    Services& app;
    const Node graph;
    const PluginDescription description;
    const ConnectionBuilder builder;
    const bool verified = true;
    double x, y;
    bool havePosition = false;
    Node addedNode;

    Node addPlugin (EngineService& ec)
    {
        auto node = app.context().plugins().getDefaultNode (description);
        if (! node.isValid())
            return ec.addPlugin (graph, description, builder, verified);
        return ec.addNode (node, graph, builder);
    }
};

class RemoveNodeAction : public juce::UndoableAction
{
public:
    explicit RemoveNodeAction (Services& a, const Node& node)
        : app (a), targetGraph (node.getParentGraph()), nodeUuid (node.getUuid())
    {
        node.getArcs (arcs);
        Node mutableNode (node);
        mutableNode.savePluginState();
        node.getRelativePosition (relativeX, relativeY);
        hasAbsolutePosition = node.hasPosition();
        if (hasAbsolutePosition)
            node.getPosition (absoluteX, absoluteY);
        nodeData = node.data().createCopy();
        Node::sanitizeRuntimeProperties (nodeData);
    }

    bool perform() override
    {
        auto& ec = *app.find<EngineService>();
        ec.removeNode (nodeUuid);
        return true;
    }

    bool undo() override
    {
        if (! targetGraph.isGraph() || ! nodeData.isValid())
            return false;

        auto& ec = *app.find<EngineService>();
        const Node newNode (nodeData, false);
        auto createdNode (ec.addNode (newNode, targetGraph, builder));

        if (! createdNode.isValid())
            return false;

        createdNode.setRelativePosition (relativeX, relativeY);
        if (hasAbsolutePosition)
            createdNode.setPosition (absoluteX, absoluteY);

        for (const auto* arc : arcs)
            ec.addConnection (arc->sourceNode, arc->sourcePort, arc->destNode, arc->destPort, targetGraph);

        return true;
    }

private:
    Services& app;
    juce::ValueTree nodeData;
    const Node targetGraph;
    const juce::Uuid nodeUuid;
    ConnectionBuilder builder;
    juce::OwnedArray<Arc> arcs;
    double relativeX = 0.5;
    double relativeY = 0.5;
    double absoluteX = 0.0;
    double absoluteY = 0.0;
    bool hasAbsolutePosition = false;

    bool isDataValid() const
    {
        return targetGraph.isGraph() && ! nodeUuid.isNull() && nodeData.isValid();
    }
};

static Node getConnectionTargetGraph (Services& app, const Node& target)
{
    if (target.isGraph())
        return target;
    if (auto session = app.context().session())
        return session->getActiveGraph();
    return Node();
}

class AutoNodeEnablementAction : public juce::UndoableAction
{
public:
    AutoNodeEnablementAction (Services& a, const Node& targetGraph, const Array<Arc>& changedArcs)
        : graph (getConnectionTargetGraph (a, targetGraph))
    {
        if (! graph.isGraph())
            return;

        Array<uint32> nodeIds;
        for (const auto& arc : changedArcs)
        {
            nodeIds.addIfNotAlreadyThere (arc.sourceNode);
            nodeIds.addIfNotAlreadyThere (arc.destNode);
        }

        for (const auto nodeId : nodeIds)
        {
            const Node node (graph.getNodeById (nodeId));
            auto* const object = node.getObject();
            if (! node.isValid() || object == nullptr || ! hasAudioPorts (node))
                continue;

            entries.add ({ node, countAudioConnections (graph, nodeId), object->isEnabled(), false });
        }
    }

    bool perform() override
    {
        for (auto& entry : entries)
        {
            entry.changed = false;
            const bool wasConnected = entry.audioConnectionsBefore > 0;
            const bool isConnected = countAudioConnections (graph, entry.node.getNodeId()) > 0;
            if (wasConnected == isConnected)
                continue;

            entry.node.setEnabled (isConnected);
            entry.changed = true;
        }
        return true;
    }

    bool undo() override
    {
        for (auto& entry : entries)
        {
            if (! entry.changed)
                continue;

            entry.node.setEnabled (entry.enabledBefore);
        }
        return true;
    }

private:
    struct Entry
    {
        Node node;
        int audioConnectionsBefore = 0;
        bool enabledBefore = true;
        bool changed = false;
    };

    const Node graph;
    Array<Entry> entries;

    static bool hasAudioPorts (const Node& node)
    {
        for (int i = 0; i < node.getNumPorts(); ++i)
            if (node.getPort (i).getType().isAudio())
                return true;
        return false;
    }

    static int countAudioConnections (const Node& graph, const uint32 nodeId)
    {
        int count = 0;
        for (int i = 0; i < graph.getNumConnections(); ++i)
        {
            const Arc arc (Node::arcFromValueTree (graph.getConnectionValueTree (i)));
            if (arc.sourceNode != nodeId && arc.destNode != nodeId)
                continue;

            const Node source (graph.getNodeById (arc.sourceNode));
            const Node dest (graph.getNodeById (arc.destNode));
            if (! source.isValid() || ! dest.isValid())
                continue;

            if (source.getPort ((int) arc.sourcePort).getType().isAudio()
                && dest.getPort ((int) arc.destPort).getType().isAudio())
                ++count;
        }
        return count;
    }
};

static void addAutoNodeEnablementAction (Services& app,
                                         OwnedArray<UndoableAction>& actions,
                                         const Node& graph,
                                         const Array<Arc>& changedArcs)
{
    if (changedArcs.isEmpty() || ! app.context().settings().autoEnableNodesWithAudioConnections())
        return;

    actions.add (new AutoNodeEnablementAction (app, graph, changedArcs));
}

class AddConnectionAction : public juce::UndoableAction

{
public:
    AddConnectionAction (Services& a, const Node& targetGraph, const uint32 sn, const uint32 sp, const uint32 dn, const uint32 dp)
        : app (a), graph (targetGraph), arc (sn, sp, dn, dp)
    {
    }

    bool perform() override
    {
        auto& ec = *app.find<EngineService>();
        if (! graph.isValid())
            ec.addConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort);
        else
            ec.addConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort, graph);
        return true;
    }

    bool undo() override
    {
        auto& ec = *app.find<EngineService>();
        if (! graph.isValid())
            ec.removeConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort);
        else
            ec.removeConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort, graph);
        return true;
    }

private:
    Services& app;
    const Node graph;
    const Arc arc;
};

class RemoveConnectionAction : public juce::UndoableAction
{
public:
    RemoveConnectionAction (Services& a, const Node& targetGraph, const uint32 sn, const uint32 sp, const uint32 dn, const uint32 dp)
        : app (a), graph (targetGraph), arc (sn, sp, dn, dp)
    {
    }

    bool perform() override
    {
        auto& ec = *app.find<EngineService>();
        if (! graph.isValid())
            ec.removeConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort);
        else
            ec.removeConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort, graph);
        return true;
    }

    bool undo() override
    {
        auto& ec = *app.find<EngineService>();
        if (! graph.isValid())
            ec.addConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort);
        else
            ec.addConnection (arc.sourceNode, arc.sourcePort, arc.destNode, arc.destPort, graph);
        return true;
    }

private:
    Services& app;
    const Node graph;
    const Arc arc;
};

//=============================================================================

void AddPluginMessage::createActions (Services& app, OwnedArray<UndoableAction>& actions) const
{
    actions.add (new AddPluginAction (app, *this));
}

void RemoveNodeMessage::createActions (Services& app, OwnedArray<UndoableAction>& actions) const
{
    if (node.isValid())
        actions.add (new RemoveNodeAction (app, node));
    for (const auto& n : nodes)
        actions.add (new RemoveNodeAction (app, n));
}

void AddConnectionMessage::createActions (Services& app, OwnedArray<UndoableAction>& actions) const
{
    jassert (usePorts()); // channel-ports not yet supported
    actions.add (new AddConnectionAction (app, target, sourceNode, sourcePort, destNode, destPort));

    Array<Arc> changedArcs;
    changedArcs.add ({ sourceNode, sourcePort, destNode, destPort });
    addAutoNodeEnablementAction (app, actions, target, changedArcs);
}

void ModifyConnectionsMessage::createActions (Services& app, OwnedArray<UndoableAction>& actions) const
{
    Array<Arc> changedArcs;
    for (const auto& connection : disconnects)
    {
        actions.add (new RemoveConnectionAction (app, target,
                                                  connection.sourceNode,
                                                  connection.sourcePort,
                                                  connection.destNode,
                                                  connection.destPort));
        changedArcs.add ({ connection.sourceNode, connection.sourcePort,
                           connection.destNode, connection.destPort });
    }

    for (const auto& connection : connections)
    {
        actions.add (new AddConnectionAction (app, target,
                                               connection.sourceNode,
                                               connection.sourcePort,
                                               connection.destNode,
                                               connection.destPort));
        changedArcs.add ({ connection.sourceNode, connection.sourcePort,
                           connection.destNode, connection.destPort });
    }

    addAutoNodeEnablementAction (app, actions, target, changedArcs);
}

void RemoveConnectionMessage::createActions (Services& app, OwnedArray<UndoableAction>& actions) const
{
    jassert (usePorts()); // channel-ports not yet supported
    actions.add (new RemoveConnectionAction (app, target, sourceNode, sourcePort, destNode, destPort));

    Array<Arc> changedArcs;
    changedArcs.add ({ sourceNode, sourcePort, destNode, destPort });
    addAutoNodeEnablementAction (app, actions, target, changedArcs);
}

} // namespace element
