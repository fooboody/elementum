// Copyright 2023 Kushview, LLC <info@kushview.net>
// Copyright 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later
// Modified by fooboody on 2026

#pragma once

#include <element/juce/core.hpp>
#include <element/juce/data_structures.hpp>

#include <element/node.hpp>

namespace element {

class Services;
class ContentView;
class Context;

class Action : public juce::UndoableAction
{
public:
    virtual ~Action() {}

protected:
    Action() {}
};

struct AppMessage : public juce::Message
{
    enum ID
    {

    };

    inline virtual void createActions (Services&, juce::OwnedArray<juce::UndoableAction>&) const {}
};

struct AddMidiDeviceMessage : public AppMessage
{
    AddMidiDeviceMessage (const juce::MidiDeviceInfo& dev, const bool isInput)
        : device (dev), inputDevice (isInput) {}
    const juce::MidiDeviceInfo device;
    const bool inputDevice;
};

/** Send this to add a preset for a node */
struct AddPresetMessage : public AppMessage
{
    AddPresetMessage (const Node& n, const juce::String& name_ = String())
        : node (n), name (name_) {}
    ~AddPresetMessage() noexcept {}
    const Node node;
    const juce::String name;
};

/** Send this to add a preset for a node */
struct SaveDefaultNodeMessage : public AppMessage
{
    SaveDefaultNodeMessage (const Node& n) : node (n) {}
    ~SaveDefaultNodeMessage() noexcept {}
    const Node node;
};

/** Send this to remove a node from the current graph */
struct RemoveNodeMessage : public AppMessage
{
    RemoveNodeMessage (const Node& n) : nodeId (n.getNodeId()), node (n) {}
    RemoveNodeMessage (const NodeArray& n) : nodeId (EL_INVALID_NODE) { nodes.addArray (n); }
    RemoveNodeMessage (const uint32 _nodeId) : nodeId (_nodeId) {}
    const uint32_t nodeId;
    const Node node;
    NodeArray nodes;

    virtual void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const;
};

/** Send this to add a new connection */
struct AddConnectionMessage : public AppMessage
{
    AddConnectionMessage (uint32_t s, int sc, uint32_t d, int dc, const Node& tgt = Node())
        : target (tgt)
    {
        sourceNode = s;
        destNode = d;
        sourceChannel = sc;
        destChannel = dc;
        sourcePort = destPort = EL_INVALID_PORT;
        jassert (useChannels());
    }

    AddConnectionMessage (uint32_t s, uint32_t sp, uint32_t d, uint32_t dp, const Node& tgt = Node())
        : target (tgt)
    {
        sourceNode = s;
        destNode = d;
        sourcePort = sp;
        destPort = dp;
        sourceChannel = destChannel = -1;
        jassert (usePorts());
    }

    uint32_t sourceNode, sourcePort, destNode, destPort;
    int sourceChannel, destChannel;

    const Node target;

    inline bool useChannels() const { return sourceChannel >= 0 && destChannel >= 0; }
    inline bool usePorts() const { return ! useChannels(); }
    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

/** Send nearby connection additions and removals as one undo transaction. */
struct ModifyConnectionsMessage : public AppMessage
{
    struct Connection
    {
        uint32_t sourceNode { 0 };
        uint32_t sourcePort { 0 };
        uint32_t destNode { 0 };
        uint32_t destPort { 0 };
    };

    explicit ModifyConnectionsMessage (const Node& tgt = Node()) : target (tgt) {}

    void add (const uint32_t sourceNode, const uint32_t sourcePort,
              const uint32_t destNode, const uint32_t destPort)
    {
        Connection connection;
        connection.sourceNode = sourceNode;
        connection.sourcePort = sourcePort;
        connection.destNode = destNode;
        connection.destPort = destPort;
        connections.add (connection);
    }

    void addDisconnect (const uint32_t sourceNode, const uint32_t sourcePort,
                        const uint32_t destNode, const uint32_t destPort)
    {
        Connection connection;
        connection.sourceNode = sourceNode;
        connection.sourcePort = sourcePort;
        connection.destNode = destNode;
        connection.destPort = destPort;
        disconnects.add (connection);
    }

    const Node target;
    juce::Array<Connection> disconnects;
    juce::Array<Connection> connections;

    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

/** Send this to remove a connection from the graph */
class RemoveConnectionMessage : public AppMessage
{
public:
    RemoveConnectionMessage (uint32 s, int sc, uint32 d, int dc, const Node& t = Node())
        : target (t)
    {
        sourceNode = s;
        destNode = d;
        sourceChannel = sc;
        destChannel = dc;
        sourcePort = destPort = EL_INVALID_PORT;
        jassert (useChannels());
    }

    RemoveConnectionMessage (uint32 s, uint32 sp, uint32 d, uint32 dp, const Node& t = Node())
        : target (t)
    {
        sourceNode = s;
        destNode = d;
        sourcePort = sp;
        destPort = dp;
        sourceChannel = destChannel = -1;
        jassert (usePorts());
    }

    uint32_t sourceNode, sourcePort, destNode, destPort;
    int sourceChannel, destChannel;
    const Node target;

    inline bool useChannels() const { return sourceChannel >= 0 && destChannel >= 0; }
    inline bool usePorts() const { return ! useChannels(); }
    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

class AddNodeMessage : public juce::Message
{
public:
    AddNodeMessage (const Node& n, const Node& t = Node(), const File& f = File())
        : node (Node::resetIds (n.data().createCopy()), false),
          target (t),
          sourceFile (f)
    {
    }

    const Node node;
    const Node target;
    ConnectionBuilder builder;
    const File sourceFile;
};

/** Send this when a plugin needs loaded into the graph */
class LoadPluginMessage : public juce::Message
{
public:
    LoadPluginMessage (const juce::PluginDescription& pluginDescription, const bool pluginVerified)
        : Message(), description (pluginDescription), verified (pluginVerified) {}
    LoadPluginMessage (const juce::PluginDescription& d, const bool v, const float rx, const float ry)
        : Message(), description (d), relativeX (rx), relativeY (ry), verified (v) {}
    ~LoadPluginMessage() {}

    /** Descriptoin of the plugin to load */
    const juce::PluginDescription description;

    /** Relative X of the node UI in a graph editor */
    const float relativeX = 0.5f;

    /** Relative X of the node UI in a graph editor */
    const float relativeY = 0.5f;

    /** Whether or not this plugin has been vetted yet */
    const bool verified;
};

struct AddPluginMessage : public AppMessage
{
    AddPluginMessage (const Node& g, const juce::PluginDescription& d, const bool v = true)
        : graph (g), description (d), verified (v)
    {
    }

    const Node graph;
    const juce::PluginDescription description;
    const bool verified;
    ConnectionBuilder builder;
    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

struct ReplaceNodeMessage : public AppMessage
{
    ReplaceNodeMessage (const Node& n, const juce::PluginDescription& d, const bool v = true)
        : graph (n.getParentGraph()), node (n), description (d), verified (v) {}
    const Node graph;
    const Node node;
    const juce::PluginDescription description;
    const bool verified;
    boost::signals2::signal<void()> success;
};

struct DuplicateNodeMessage : public AppMessage
{
    DuplicateNodeMessage (const Node& n, const Node& target = Node())
        : node (n), targetGraph (target) {}
    DuplicateNodeMessage() {}

    const Node node;
    const Node targetGraph;
    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

struct DisconnectNodeMessage : public AppMessage
{
    DisconnectNodeMessage (const Node& n, const bool i = true, const bool o = true, const bool a = true, const bool m = true)
        : node (n), inputs (i), outputs (o), audio (a), midi (m) {}
    DisconnectNodeMessage()
        : inputs (true), outputs (true), audio (true), midi (true) {}

    const Node node;
    const bool inputs, outputs;
    const bool audio, midi;
    void createActions (Services& app, juce::OwnedArray<juce::UndoableAction>& actions) const override;
};

struct FinishedLaunchingMessage : public AppMessage
{
    FinishedLaunchingMessage() {}
    ~FinishedLaunchingMessage() {}
};

struct ChangeBusesLayout : public AppMessage
{
    ChangeBusesLayout (const Node& n, const juce::AudioProcessor::BusesLayout& l)
        : node (n), layout (l) {}
    const Node node;
    const juce::AudioProcessor::BusesLayout layout;
    std::function<void()> onFinished;
};

struct OpenSessionMessage : public AppMessage
{
    OpenSessionMessage (const File& f) : file (f) {}
    ~OpenSessionMessage() {}
    const File file;
};

//=============================================================================
struct WorkspaceOpenFileMessage : public AppMessage
{
    WorkspaceOpenFileMessage (const File& f) : file (f) {}
    ~WorkspaceOpenFileMessage() noexcept {}
    const File file;
};

struct ReloadMainContentMessage : public AppMessage
{
    explicit ReloadMainContentMessage (const String& t = String()) : type (t) {}
    ~ReloadMainContentMessage() noexcept {}
    const String type;
};

} // namespace element
