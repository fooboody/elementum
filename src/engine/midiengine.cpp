// Copyright 2023 Kushview, LLC <info@kushview.net>
// Copyright 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later
// Modified by fooboody on 2026

#include <element/juce/audio_basics.hpp>
#include <element/juce/audio_devices.hpp>
#include <element/juce/data_structures.hpp>

#include <element/settings.hpp>
#include <element/tags.hpp>

#include "engine/midiengine.hpp"

namespace element {
using juce::MidiInput;
using juce::MidiInputCallback;
using juce::MidiMessage;
using juce::ValueTree;
using juce::XmlElement;

MidiEngine* MidiEngine::currentInstance = nullptr;

//==============================================================================
void MidiEngine::applySettings (Settings& settings)
{
    // Refresh MIDI device list before applying settings
    const auto midiInputs = MidiInput::getAvailableDevices();
    const auto midiOutputs = MidiOutput::getAvailableDevices();
    juce::ignoreUnused (midiOutputs);

    midiInsFromXml.clear();
    oscReceiverActive = false;

    if (auto xml = std::unique_ptr<XmlElement> (settings.getUserSettings()->getXmlValue (Settings::midiEngineKey)))
    {
        const auto data = ValueTree::fromXml (*xml);
        for (int i = 0; i < data.getNumChildren(); ++i)
        {
            const auto child (data.getChild (i));
            if (child.hasType (tags::input))
            {
                if (child[tags::identifier].toString() == oscReceiverDeviceIdentifier)
                {
                    oscReceiverActive = (bool) child[tags::active];
                    continue;
                }
                if (auto* const holder = getMidiInput (child[tags::identifier], true))
                {
                    holder->active = false; // open but not active initially
                    if ((bool) child[tags::active])
                        midiInsFromXml.add (child[tags::identifier]);
                }
            }
        }

        for (auto& m : midiInputs)
            setMidiInputEnabled (m, midiInsFromXml.contains (m.identifier));

        MidiDeviceInfo info;
        info.name = data["defaultMidiOutput"].toString();
        info.identifier = data["defaultMidiOutputID"].toString();
        setDefaultMidiOutput (info);
    }
}

void MidiEngine::writeSettings (Settings& settings)
{
    ValueTree data ("MidiSettings");
    StringArray writtenInputIDs;

    for (auto* const holder : openMidiInputs)
    {
        const auto identifier = holder->input->getIdentifier();
        ValueTree input (tags::input);
        input.setProperty (tags::name, holder->input->getName(), nullptr)
            .setProperty (tags::identifier, identifier, nullptr)
            // midiInsFromXml is the persistent user choice. holder->active is
            // only the current runtime routing state and may be false while a
            // device is being recovered.
            .setProperty (tags::active, midiInsFromXml.contains (identifier), nullptr);
        data.appendChild (input, nullptr);
        writtenInputIDs.addIfNotAlreadyThere (identifier);
    }

    {
        ValueTree input (tags::input);
        input.setProperty (tags::name, oscReceiverDeviceName, nullptr)
            .setProperty (tags::identifier, oscReceiverDeviceIdentifier, nullptr)
            .setProperty (tags::active, oscReceiverActive, nullptr);
        data.appendChild (input, nullptr);
    }

    for (const auto& identifier : midiInsFromXml)
    {
        if (writtenInputIDs.contains (identifier))
            continue;

        MidiDeviceInfo info;
        info.identifier = identifier;
        for (const auto& device : MidiInput::getAvailableDevices())
        {
            if (device.identifier == identifier)
            {
                info.name = device.name;
                break;
            }
        }

        ValueTree input (tags::input);
        input.setProperty (tags::name, info.name, nullptr)
            .setProperty (tags::identifier, info.identifier, nullptr)
            .setProperty (tags::active, true, nullptr);
        data.appendChild (input, nullptr);
    }

    data.setProperty ("defaultMidiOutput", defaultMidiOutputName, nullptr);
    data.setProperty ("defaultMidiOutputID", defaultMidiOutputID, nullptr);

    if (auto xml = std::unique_ptr<XmlElement> (data.createXml()))
        settings.getUserSettings()->setValue (Settings::midiEngineKey, xml.get());
}

//==============================================================================
class MidiEngine::CallbackHandler : public juce::AudioIODeviceCallback,
                                    public juce::MidiInputCallback,
                                    public juce::AudioIODeviceType::Listener
{
public:
    CallbackHandler (MidiEngine& me) noexcept : owner (me) {}

private:
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override
    {
        ignoreUnused (inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, context);
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        ignoreUnused (device);
    }

    void audioDeviceStopped() override
    {
        // noop
    }

    void audioDeviceError (const String& message) override
    {
        // noop
    }

    void handleIncomingMidiMessage (MidiInput* source, const MidiMessage& message) override
    {
        owner.handleIncomingMidiMessageInt (source, message);
    }

    void audioDeviceListChanged() override
    {
        // noop
    }

    MidiEngine& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CallbackHandler)
};

void MidiEngine::MidiInputHolder::handleIncomingMidiMessage (MidiInput* source, const MidiMessage& message)
{
    if (message.isActiveSense())
        return;

    jassert (source == input.get());
    if (active)
        engine.dispatchIncomingMidiMessage (input->getIdentifier(), input.get(), message);
    else
    {
        const ScopedLock sl (engine.midiCallbackLock);
        for (auto& mc : engine.midiCallbacks)
            if (mc.consumer && (mc.device.isEmpty() || mc.device == input->getIdentifier()))
                if (auto* extended = dynamic_cast<MidiInputCallbackWithIdentifier*> (mc.callback))
                    extended->handleIncomingMidiMessage (input->getIdentifier(), message);
                else
                    mc.callback->handleIncomingMidiMessage (input.get(), message);
    }
}

//==============================================================================
MidiEngine::MidiEngine()
{
    currentInstance = this;
    callbackHandler.reset (new CallbackHandler (*this));

    for (const auto& device : MidiInput::getAvailableDevices())
        knownMidiInputIDs.addIfNotAlreadyThere (device.identifier);
    for (const auto& device : MidiOutput::getAvailableDevices())
        knownMidiOutputIDs.addIfNotAlreadyThere (device.identifier);

    deviceListConnection = juce::MidiDeviceListConnection::make ([this] { handleDeviceListChanged(); });
}

MidiEngine::~MidiEngine()
{
    deviceListConnection.reset();
    callbackHandler.reset (nullptr);
    if (currentInstance == this)
        currentInstance = nullptr;
}

void MidiEngine::handleDeviceListChanged()
{
    StringArray inputIDs;
    for (const auto& device : MidiInput::getAvailableDevices())
        inputIDs.addIfNotAlreadyThere (device.identifier);

    StringArray outputIDs;
    for (const auto& device : MidiOutput::getAvailableDevices())
        outputIDs.addIfNotAlreadyThere (device.identifier);

    const auto listsDiffer = [] (const StringArray& lhs, const StringArray& rhs)
    {
        if (lhs.size() != rhs.size())
            return true;

        for (const auto& identifier : lhs)
            if (! rhs.contains (identifier))
                return true;

        return false;
    };

    const bool inputListChanged = listsDiffer (inputIDs, knownMidiInputIDs);
    const bool outputListChanged = listsDiffer (outputIDs, knownMidiOutputIDs);
    knownMidiInputIDs = inputIDs;
    knownMidiOutputIDs = outputIDs;

    bool defaultOutputAvailable = false;
    if (defaultMidiOutputID.isNotEmpty())
        defaultOutputAvailable = outputIDs.contains (defaultMidiOutputID);

    // Use the full recovery path only when an input changed or the selected
    // default output crossed an available/unavailable boundary. An unrelated
    // output-list change still notifies Output nodes without restarting every
    // MIDI input.
    if (inputListChanged
        || (defaultMidiOutputID.isNotEmpty() && defaultOutputAvailable != (defaultMidiOutput != nullptr)))
    {
        refreshMidiInputs();
    }
    else if (outputListChanged)
    {
        sendChangeMessage();
    }
}

//==============================================================================
MidiEngine::MidiInputHolder* MidiEngine::getMidiInput (const String& identifier, bool openIfNotAlready)
{
    for (auto* const holder : openMidiInputs)
        if (holder->input && holder->input->getIdentifier() == identifier)
            return holder;

    if (! openIfNotAlready)
        return nullptr;

    int index = 0;
    bool found = false;
    for (const auto& dev : MidiInput::getAvailableDevices())
    {
        if (identifier != dev.identifier)
        {
            ++index;
            continue;
        }
        found = true;
        break;
    }

    juce::ignoreUnused (index);

    if (found)
    {
        std::unique_ptr<MidiInputHolder> holder;
        holder.reset (new MidiInputHolder (*this));
        if (auto midiIn = MidiInput::openDevice (identifier, holder.get()))
        {
            holder->input.reset (midiIn.release());
            holder->input->start();
            return openMidiInputs.add (holder.release());
        }
    }

    return nullptr;
}

//==============================================================================
void MidiEngine::setMidiInputEnabled (const MidiDeviceInfo& device, const bool enabled)
{
    if (device.identifier == oscReceiverDeviceIdentifier)
    {
        if (enabled != oscReceiverActive)
        {
            oscReceiverActive = enabled;
            sendChangeMessage();
        }
        return;
    }

    const bool wasEnabled = isMidiInputEnabled (device);

    // This list is the persistent user preference. Keep it independent from
    // the transient runtime state of the MIDI handle.
    if (enabled)
        midiInsFromXml.addIfNotAlreadyThere (device.identifier);
    else
        midiInsFromXml.removeString (device.identifier);

    if (enabled != wasEnabled)
    {
        if (enabled)
        {
            if (auto* holder = getMidiInput (device.identifier, true))
                holder->active = true;
        }
        else
        {
            if (auto* holder = getMidiInput (device.identifier, false))
                holder->active = false;
        }

        sendChangeMessage();
    }
}

bool MidiEngine::isMidiInputEnabled (const MidiDeviceInfo& device) const
{
    if (device.identifier == oscReceiverDeviceIdentifier)
        return oscReceiverActive;

    for (auto* mi : openMidiInputs)
        if (mi->input != nullptr && mi->input->getIdentifier() == device.identifier && mi->active)
            return true;

    return false;
}

void MidiEngine::addMidiInputCallback (MidiInputCallback* callbackToAdd, bool consumer)
{
    addMidiInputCallback (MidiDeviceInfo(), callbackToAdd, consumer);
}

void MidiEngine::addMidiInputCallback (const String& identifier, MidiInputCallback* callback, bool consumer)
{
    MidiDeviceInfo info;
    info.identifier = identifier;
    addMidiInputCallback (info, callback, consumer);
}

void MidiEngine::addMidiInputCallback (const MidiDeviceInfo& device, MidiInputCallback* callbackToAdd, bool consumer)
{
    removeMidiInputCallback (device, callbackToAdd);

    if (device.identifier.isEmpty() || isMidiInputEnabled (device) || consumer)
    {
        if (consumer)
        {
            if (auto* holder = getMidiInput (device.identifier, true))
                ignoreUnused (holder);
        }

        MidiCallbackInfo mc;
        mc.device = device.identifier;
        mc.callback = callbackToAdd;
        mc.consumer = consumer;

        const ScopedLock sl (midiCallbackLock);
        midiCallbacks.add (mc);
    }
}

void MidiEngine::removeMidiInputCallback (const MidiDeviceInfo& device, MidiInputCallback* callbackToRemove)
{
    for (int i = midiCallbacks.size(); --i >= 0;)
    {
        auto& mc = midiCallbacks.getReference (i);

        if (mc.callback == callbackToRemove && mc.device == device.identifier)
        {
            const ScopedLock sl (midiCallbackLock);
            midiCallbacks.remove (i);
        }
    }
}

void MidiEngine::removeMidiInputCallback (MidiInputCallback* callbackToRemove)
{
    for (int i = midiCallbacks.size(); --i >= 0;)
    {
        auto& mc = midiCallbacks.getReference (i);

        if (mc.callback == callbackToRemove)
        {
            const ScopedLock sl (midiCallbackLock);
            midiCallbacks.remove (i);
        }
    }
}

void MidiEngine::handleIncomingMidiMessageInt (MidiInput* source, const MidiMessage& message)
{
    if (! message.isActiveSense())
        dispatchIncomingMidiMessage (source != nullptr ? source->getIdentifier() : String(), source, message);
}

void MidiEngine::dispatchIncomingMidiMessage (const String& identifier, MidiInput* source,
                                              const MidiMessage& message)
{
    const ScopedLock sl (midiCallbackLock);
    for (auto& mc : midiCallbacks)
        if (mc.consumer || mc.device.isEmpty() || mc.device == identifier)
            if (auto* extended = dynamic_cast<MidiInputCallbackWithIdentifier*> (mc.callback))
                extended->handleIncomingMidiMessage (identifier, message);
            else
                mc.callback->handleIncomingMidiMessage (source, message);
}

void MidiEngine::routeOscReceiverMidiMessage (const MidiMessage& message)
{
    if (currentInstance != nullptr && currentInstance->oscReceiverActive)
        currentInstance->dispatchIncomingMidiMessage (oscReceiverDeviceIdentifier, nullptr, message);
}

void MidiEngine::processMidiBuffer (const MidiBuffer& buffer, int nframes, double sampleRate)
{
    MidiMessage message;
    const double timeNow = 1.5 + Time::getMillisecondCounterHiRes();

    const ScopedLock sl (midiCallbackLock);
    for (auto m : buffer)
    {
        if (m.samplePosition >= nframes)
            break;
        message = m.getMessage();
        message.setTimeStamp (timeNow + (1000.0 * (static_cast<double> (m.samplePosition) / sampleRate)));
        for (auto& mc : midiCallbacks)
            mc.callback->handleIncomingMidiMessage (nullptr, message);
    }
}

int MidiEngine::getNumActiveMidiInputs() const
{
    int total = oscReceiverActive ? 1 : 0;
    for (const auto& dev : MidiInput::getAvailableDevices())
        if (isMidiInputEnabled (dev))
            ++total;
    return total;
}

void MidiEngine::refreshMidiInputs()
{
    struct InputState
    {
        MidiDeviceInfo device;
        bool active = false;
    };

    Array<InputState> inputsToReopen;
    inputsToReopen.ensureStorageAllocated (openMidiInputs.size());

    auto addInputToReopen = [&inputsToReopen] (const String& identifier, const String& name, bool active)
    {
        for (const auto& input : inputsToReopen)
            if (input.device.identifier == identifier)
                return;

        MidiDeviceInfo device;
        device.identifier = identifier;
        device.name = name;
        inputsToReopen.add ({ device, active });
    };

    for (const auto* holder : openMidiInputs)
    {
        if (holder == nullptr || holder->input == nullptr)
            continue;

        const auto device = holder->input->getDeviceInfo();
        addInputToReopen (device.identifier, device.name, holder->active);
    }

    // Preserve enabled inputs and consumer-owned inputs even when the device
    // is already absent when the list-change callback arrives. They will be
    // reopened by a later device-list change when they become available.
    for (const auto& identifier : midiInsFromXml)
        addInputToReopen (identifier, String(), true);

    {
        const ScopedLock sl (midiCallbackLock);
        for (const auto& callback : midiCallbacks)
            if (callback.consumer && callback.device.isNotEmpty())
                addInputToReopen (callback.device, String(), false);
    }

    // Do not hold midiCallbackLock while stopping or destroying a MidiInput:
    // that may wait for a callback already in flight, while that callback takes
    // midiCallbackLock before it dispatches messages.
    for (auto* holder : openMidiInputs)
        if (holder != nullptr && holder->input != nullptr)
            holder->input->stop();

    openMidiInputs.clear (true);

   #if JUCE_WINDOWS
    // MidiInput instances are backed by JUCE's process-wide UMP Endpoints
    // singleton on Windows. Recreating inputs alone therefore leaves the same
    // native endpoint cache and session alive. Release all Element-owned MIDI
    // handles before recreating that singleton, which is the closest safe
    // equivalent to the clean MIDI state obtained on an application restart.
    std::unique_ptr<MidiOutput> oldMidiOutput;
    {
        const ScopedLock sl (midiOutputLock);
        oldMidiOutput.swap (defaultMidiOutput);
    }

    if (oldMidiOutput != nullptr)
    {
        oldMidiOutput->stopBackgroundThread();
        oldMidiOutput.reset();
    }

    juce::universal_midi_packets::Endpoints::deleteInstance();
   #endif

    for (const auto& input : inputsToReopen)
    {
        if (auto* holder = getMidiInput (input.device.identifier, true))
            holder->active = input.active;
        else
            Logger::writeToLog ("[element] unable to refresh MIDI input " + input.device.name
                                + " (" + input.device.identifier + ")");
    }

   #if JUCE_WINDOWS
    if (defaultMidiOutputID.isNotEmpty())
    {
        if (auto midiOutput = MidiOutput::openDevice (defaultMidiOutputID))
        {
            midiOutput->startBackgroundThread();
            const ScopedLock sl (midiOutputLock);
            defaultMidiOutput = std::move (midiOutput);
        }
        else
        {
            Logger::writeToLog ("[element] unable to refresh MIDI output " + defaultMidiOutputName
                                + " (" + defaultMidiOutputID + ")");
        }
    }
   #endif

    sendChangeMessage();

}

//==============================================================================

void MidiEngine::setDefaultMidiOutput (const MidiDeviceInfo& device)
{
    if (defaultMidiOutputID != device.identifier)
    {
        std::unique_ptr<MidiOutput> newMidiOut;

        if (device.identifier.isNotEmpty())
            newMidiOut = MidiOutput::openDevice (device.identifier);

        if (newMidiOut)
            newMidiOut->startBackgroundThread();

        {
            ScopedLock sl (midiOutputLock);
            defaultMidiOutput.swap (newMidiOut);
        }

        if (newMidiOut) // is now the old output
        {
            newMidiOut->stopBackgroundThread();
            newMidiOut.reset();
        }

        defaultMidiOutputName = device.name;
        defaultMidiOutputID = device.identifier;

        sendChangeMessage();
    }
}

} // namespace element
