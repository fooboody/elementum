// SPDX-FileCopyrightText: Copyright (C) Kushview, LLC.
// SPDX-FileCopyrightText: Copyright (C) 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <thread>
#include <element/application.hpp>
#include <element/services.hpp>
#include <element/version.hpp>
#include <element/context.hpp>
#include <element/devices.hpp>
#include <element/plugins.hpp>
#include <element/plugindetails.hpp>
#include <element/nodefactory.hpp>
#include <element/settings.hpp>
#include <element/ui.hpp>
#include <element/ui/commands.hpp>
#include <element/ui/commands.hpp>
#include <element/ui/content.hpp>
#include <element/ui/mainwindow.hpp>
#include <element/ui/updater.hpp>

#include "engine/clapprovider.hpp"
#include "engine/internalformat.hpp"
#include "engine/midiengine.hpp"
#include "scripting.hpp"
#include <element/datapath.hpp>
#include "services/sessionservice.hpp"
#include "log.hpp"
#include "messages.hpp"
#include "auth.hpp"
#include "utils.hpp"

/** Define to force the application to behave as if running for the first time.
    When enabled (set to 1), the application will:
    - Skip checking for existing settings files
    - Initialize default plugin scan paths
    - Perform other first-run setup tasks
    
    This is useful for testing first-run behavior without clearing user settings.
*/
#ifndef ELEMENT_FIRST_RUN
#define ELEMENT_FIRST_RUN 0
#endif

using namespace juce;

namespace element {

static std::atomic<int> sCanShutdown { 0 };

namespace {
constexpr auto profileInstanceMessagePrefix = "elementum-profile-instance:";

struct ParsedLaunchArguments
{
    StringArray remaining;
    File applicationDataDirectory;
    String profileDirectoryError;
};

String normaliseProfileIdentifier (const File& directory)
{
    auto identifier = directory.getLinkedTarget().getFullPathName();
#if JUCE_WINDOWS
    identifier = identifier.toLowerCase();
#endif
    return identifier;
}

Result prepareProfileDirectory (const File& directory)
{
    if (! directory.isDirectory())
        return Result::fail ("The requested profile directory does not exist:\n" + directory.getFullPathName());

    const auto settingsFile = Settings::getSettingsFileForDataDirectory (directory);
    if (settingsFile.exists() && ! settingsFile.existsAsFile())
        return Result::fail ("The profile settings path is not a file:\n" + settingsFile.getFullPathName());

    if (! settingsFile.existsAsFile())
    {
        if (! settingsFile.hasWriteAccess())
            return Result::fail ("Elementum cannot create the profile settings file:\n" + settingsFile.getFullPathName());

        const auto createResult = settingsFile.create();
        if (createResult.failed() || ! settingsFile.existsAsFile())
            return Result::fail ("Elementum could not create the profile settings file:\n"
                                 + settingsFile.getFullPathName()
                                 + "\n\n"
                                 + createResult.getErrorMessage());
    }

    if (! settingsFile.hasWriteAccess())
        return Result::fail ("Elementum cannot write to the profile settings file:\n" + settingsFile.getFullPathName());

    return Result::ok();
}

ParsedLaunchArguments parseLaunchArguments (const String& commandLine)
{
    ParsedLaunchArguments parsed;
    const auto tokens = StringArray::fromTokens (commandLine, true);

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];
        String value;
        bool isProfileDirectoryOption = false;

        if (token == "--profile-dir")
        {
            isProfileDirectoryOption = true;
            if (i + 1 < tokens.size())
                value = tokens[++i].unquoted();
        }
        else if (token.startsWith ("--profile-dir="))
        {
            isProfileDirectoryOption = true;
            value = token.fromFirstOccurrenceOf ("=", false, false).unquoted();
        }

        if (isProfileDirectoryOption)
        {
            parsed.applicationDataDirectory = File();
            parsed.profileDirectoryError.clear();

            if (value.isEmpty())
            {
                parsed.profileDirectoryError = "No directory was provided for --profile-dir.";
                continue;
            }

            const auto directory = File::isAbsolutePath (value)
                                       ? File (value)
                                       : File::getCurrentWorkingDirectory().getChildFile (value);
            const auto profileResult = prepareProfileDirectory (directory);
            if (profileResult.wasOk())
                parsed.applicationDataDirectory = directory.getLinkedTarget();
            else
                parsed.profileDirectoryError = profileResult.getErrorMessage();

            continue;
        }

        parsed.remaining.add (token);
    }

        return parsed;
}

struct PluginDetailsLaunchArguments
{
    bool writeText = false;
    bool writeJson = false;
    String sourcePath;
    String error;

    bool isRequested() const noexcept { return writeText || writeJson; }
};

PluginDetailsLaunchArguments parsePluginDetailsLaunchArguments (const String& commandLine)
{
    PluginDetailsLaunchArguments parsed;
    const auto tokens = StringArray::fromTokens (commandLine, true);

    for (int i = 0; i < tokens.size() - 1; ++i)
    {
        if (tokens[i] == "--details")
            parsed.writeText = true;
        else if (tokens[i] == "--details-json")
            parsed.writeJson = true;
    }

    if (tokens.size() > 0)
    {
        const auto& finalToken = tokens.getReference (tokens.size() - 1);
        if (finalToken == "--details")
            parsed.writeText = true;
        else if (finalToken == "--details-json")
            parsed.writeJson = true;
        else
            parsed.sourcePath = finalToken.unquoted();
    }

    if (! parsed.isRequested())
        return parsed;

    if (parsed.sourcePath.isEmpty())
        parsed.error = "No plug-in path was provided for --details or --details-json.";

    return parsed;
}

File getPluginDetailsOutputDirectory()
{
    return DataPath::defaultUserDataPath().getChildFile ("PluginDetails");
}

struct PluginDetailsOutput
{
    File destination;
    String content;
    File temporary;
    bool finalized = false;
};

String getPluginDetailFormatForExtension (const File& source)
{
    const auto extension = source.getFileExtension().toLowerCase();
    if (extension == ".vst3")
        return "VST3";
    if (extension == ".vst" || extension == ".dll")
        return "VST";
    if (extension == ".aaxplugin")
        return "AAX";
    if (extension == ".component")
        return "AudioUnit";
    if (extension == ".lv2")
        return "LV2";
    if (extension == ".clap")
        return "CLAP";
    return {};
}

bool isNestedInsidePluginDetailsBundle (const File& file, const File& searchRoot)
{
    for (auto parent = file.getParentDirectory(); parent.exists();)
    {
        if (parent == searchRoot)
            return false;
        if (getPluginDetailFormatForExtension (parent).isNotEmpty())
            return true;

        const auto next = parent.getParentDirectory();
        if (next == parent)
            break;
        parent = next;
    }

    return false;
}

StringArray getPluginDetailIdentifiers (const File& source, const String& formatName)
{
    StringArray identifiers;
    const auto requestedFormat = getPluginDetailFormatForExtension (source);

    if (requestedFormat.isNotEmpty())
    {
        if (formatName == requestedFormat)
            identifiers.add (source.getFullPathName());
    }
    else if (source.isDirectory())
    {
        Array<File> candidates;
        source.findChildFiles (candidates, File::findFilesAndDirectories, true);
        for (const auto& candidate : candidates)
        {
            if (getPluginDetailFormatForExtension (candidate) == formatName
                && ! isNestedInsidePluginDetailsBundle (candidate, source))
                identifiers.add (candidate.getFullPathName());
        }
    }

    identifiers.removeDuplicates (false);
    identifiers.removeEmptyStrings();
    return identifiers;
}

Result collectPluginDetails (PluginManager& plugins,
                             const File& source,
                             OwnedArray<PluginDescription>& descriptions,
                             const std::function<void (PluginScanner*)>& scannerReady)
{
    std::unique_ptr<PluginScanner> scanner (plugins.createAudioPluginScanner());
    // Details export handles one explicit target at a time. Do not leave a
    // command-line caller waiting behind a plug-in that cannot report metadata.
    scanner->setLaunchTimeout (5000);
    scanner->setPerPluginTimeout (5000);
    scannerReady (scanner.get());
    StringArray formats;

    for (int i = 0; i < plugins.getAudioPluginFormats().getNumFormats(); ++i)
        formats.addIfNotAlreadyThere (plugins.getAudioPluginFormats().getFormat (i)->getName());

    for (auto* provider : plugins.getNodeFactory().providers())
        formats.addIfNotAlreadyThere (provider->format());

    for (const auto& format : formats)
    {
        const auto identifiers = getPluginDetailIdentifiers (source, format);
        for (const auto& identifier : identifiers)
        {
            OwnedArray<PluginDescription> result;
            const auto scanResult = scanner->retrievePluginDescriptions (format, identifier, result);
            if (scanResult.failed())
            {
                scannerReady (nullptr);
                return scanResult;
            }

            for (auto* description : result)
                descriptions.add (new PluginDescription (*description));
        }
    }

    scannerReady (nullptr);
    return descriptions.isEmpty()
               ? Result::fail ("No supported plug-ins were found at:\n" + source.getFullPathName())
               : Result::ok();
}

Result writePluginDetailsOutputs (const OwnedArray<PluginDescription>& descriptions,
                                  const PluginDetailsLaunchArguments& options)
{
    Array<PluginDetailsOutput> outputs;
    const auto directory = getPluginDetailsOutputDirectory();
    const bool directoryExisted = directory.exists();
    const auto cleanEmptyNewDirectory = [&] {
        if (! directoryExisted && directory.isDirectory()
            && directory.getNumberOfChildFiles (File::findFilesAndDirectories) == 0)
            directory.deleteRecursively();
    };

    for (const auto* description : descriptions)
    {
        const auto identifier = description->createIdentifierString();
        if (identifier.isEmpty())
            return Result::fail ("A plug-in description has no identifier and cannot be exported.");

        if (options.writeText)
            outputs.add ({ directory.getChildFile (identifier).withFileExtension ("txt"),
                           getPluginDetailsText (*description), {} });
        if (options.writeJson)
            outputs.add ({ directory.getChildFile (identifier).withFileExtension ("json"),
                           getPluginDetailsJsonText (*description), {} });
    }

    for (const auto& output : outputs)
    {
        if (output.destination.exists())
            return Result::fail ("A Plugin Details output file already exists:\n" + output.destination.getFullPathName());
    }

    if (! directory.createDirectory() || ! directory.isDirectory())
        return Result::fail ("Elementum could not create the PluginDetails output directory:\n" + directory.getFullPathName());

    for (auto& output : outputs)
    {
        output.temporary = directory.getNonexistentChildFile (".plugin-details-" + String (Random::getSystemRandom().nextInt()), "tmp", false);
        FileOutputStream stream (output.temporary);
        if (! stream.openedOk())
        {
            for (const auto& pending : outputs)
                pending.temporary.deleteFile();
            cleanEmptyNewDirectory();
            return Result::fail ("Elementum could not write the Plugin Details output file:\n" + output.destination.getFullPathName());
        }

        stream.writeText (output.content, false, false, "\n");
        stream.flush();
        if (stream.getStatus().failed())
        {
            for (const auto& pending : outputs)
                pending.temporary.deleteFile();
            cleanEmptyNewDirectory();
            return Result::fail ("Elementum could not write the Plugin Details output file:\n" + output.destination.getFullPathName());
        }
    }

    for (auto& output : outputs)
    {
        if (! output.temporary.moveFileTo (output.destination))
        {
            for (const auto& pending : outputs)
            {
                pending.temporary.deleteFile();
                if (pending.finalized)
                    pending.destination.deleteFile();
            }
            cleanEmptyNewDirectory();
            return Result::fail ("Elementum could not finalize the Plugin Details output file:\n" + output.destination.getFullPathName());
        }
        output.finalized = true;
    }

    return Result::ok();
}

bool isPluginDetailsScanFailure (const Result& result)
{
    const auto message = result.getErrorMessage();
    return message.startsWith ("The plug-in scanner timed out or stopped")
        || message.startsWith ("The plug-in scanner could not be started")
        || message.startsWith ("The plug-in scan was cancelled");
}

String encodeProfileInstanceMessage (const String& profileIdentifier,
                                     const StringArray& arguments)
{
    return String (profileInstanceMessagePrefix)
        + Base64::toBase64 (profileIdentifier)
        + "."
        + Base64::toBase64 (arguments.joinIntoString ("\n"));
}

bool decodeProfileInstanceMessage (const String& message,
                                   String& profileIdentifier,
                                   StringArray& arguments)
{
    if (! message.startsWith (profileInstanceMessagePrefix))
        return false;

    const auto encoded = message.fromFirstOccurrenceOf (profileInstanceMessagePrefix, false, false);
    const auto separator = encoded.indexOfChar ('.');
    if (separator < 0)
        return false;

    MemoryOutputStream profileData;
    MemoryOutputStream argumentsData;
    if (! Base64::convertFromBase64 (profileData, encoded.substring (0, separator))
        || ! Base64::convertFromBase64 (argumentsData, encoded.substring (separator + 1)))
        return false;

    profileIdentifier = profileData.toString();
    arguments = StringArray::fromLines (argumentsData.toString());
    return profileIdentifier.isNotEmpty();
}
} // namespace

class Application::PluginDetailsExportThread final : public Thread
{
public:
    PluginDetailsExportThread (Application& ownerToUse, PluginDetailsLaunchArguments optionsToUse)
        : Thread ("Plugin Details Export"), owner (ownerToUse), options (std::move (optionsToUse)) {}

    void requestCancel()
    {
        cancelRequested = true;
        if (auto* scanner = activeScanner.load())
            scanner->cancel();
    }

private:
    void run() override
    {
        WaitableEvent completed;
        std::thread watchdog ([this, &completed] {
            if (! completed.wait (5000))
                requestCancel();
        });

        Result result = Result::ok();
        if (options.error.isNotEmpty())
        {
            result = Result::fail (options.error);
        }
        else
        {
            const auto source = File::isAbsolutePath (options.sourcePath)
                                    ? File (options.sourcePath)
                                    : File::getCurrentWorkingDirectory().getChildFile (options.sourcePath);
            if (! source.exists())
            {
                result = Result::fail ("The requested plug-in path does not exist:\n" + source.getFullPathName());
            }
            else
            {
                PluginManager plugins;
                plugins.getNodeFactory().add (new CLAPProvider());
                plugins.addDefaultFormats();

                OwnedArray<PluginDescription> descriptions;
                result = collectPluginDetails (plugins, source, descriptions, [this] (PluginScanner* scanner) {
                    activeScanner = scanner;
                    if (cancelRequested.load() && scanner != nullptr)
                        scanner->cancel();
                });

                if (result.wasOk())
                    result = writePluginDetailsOutputs (descriptions, options);
            }
        }

        activeScanner = nullptr;
        completed.signal();
        watchdog.join();
        MessageManager::callAsync ([this, result] { owner.finishPluginDetailsExport (result); });
    }

    Application& owner;
    PluginDetailsLaunchArguments options;
    std::atomic<PluginScanner*> activeScanner { nullptr };
    std::atomic<bool> cancelRequested { false };
};

class Startup : public ActionBroadcaster
{
public:
    Startup (Context& w)
        : world (w), isFirstRun (false) {}
    ~Startup() {}

    void launchApplication()
    {
        DataPath::initializeDefaultLocation();

        [[maybe_unused]] Settings& settings (world.settings());
#if ELEMENT_FIRST_RUN
        isFirstRun = true;
#else
        isFirstRun = ! settings.getUserSettings()->getFile().existsAsFile();
#endif
        setupLogging();
        setupKeyMappings();
        setupAudioEngine();
        setupPlugins();
        setupMidiEngine();
        setupScripting();
        setupRepos();

        sendActionMessage ("finishedLaunching");
    }

private:
    friend class Application;
    Context& world;
    bool isFirstRun;

    void setupAudioEngine()
    {
        auto& settings = world.settings();
        DeviceManager& devices (world.devices());
        for (const auto& tp : devices.getAvailableDeviceTypes())
            tp->scanForDevices();

        AudioEnginePtr engine = world.audio();
        engine->applySettings (settings);

        auto* props = settings.getUserSettings();

        String error = "No device found at startup";
        if (auto dxml = props->getXmlValue ("devices"))
        {
            error = devices.initialise (DeviceManager::maxAudioChannels,
                                        DeviceManager::maxAudioChannels,
                                        dxml.get(),
                                        true,
                                        "*",
                                        nullptr);
            if (error.isNotEmpty())
            {
                auto setup = devices.getAudioDeviceSetup();
                error = devices.setAudioDeviceSetup (setup, true);
            }
        }

        if (error.isNotEmpty())
        {
#if JUCE_WINDOWS
            devices.setCurrentAudioDeviceType ("Windows Audio (Low Latency Mode)", true);
#endif
            error = devices.initialiseWithDefaultDevices (DeviceManager::maxAudioChannels,
                                                          DeviceManager::maxAudioChannels);
        }

        if (error.isNotEmpty())
        {
            Logger::writeToLog (error);
        }
    }

    void setupMidiEngine()
    {
#if ! JUCE_LINUX
        auto& midi = world.midi();
        midi.applySettings (world.settings());
#endif
    }

    void setupKeyMappings()
    {
        auto* const props = world.settings().getUserSettings();
        auto* const keymp = world.services().find<GuiService>()->commands().getKeyMappings();
        if (props && keymp)
        {
            std::unique_ptr<XmlElement> xml;
            xml = props->getXmlValue (Settings::keymappingsKey);
            if (xml != nullptr)
                keymp->restoreFromXml (*xml);
            xml = nullptr;
        }
    }

    void setupPlugins()
    {
        auto& settings (world.settings());
        auto& plugins (world.plugins());
        plugins.restoreUserPlugins (settings);
        plugins.setPropertiesFile (settings.getUserSettings());

        if (isFirstRun)
        {
            auto props = settings.getUserSettings();
            for (const auto& formatName : Util::compiledAudioPluginFormats())
                props->setValue (Settings::lastPluginScanPathPrefix + formatName,
                                 plugins.defaultSearchPath (formatName).toString());
        }

        plugins.scanInternalPlugins();
        plugins.searchUnverifiedPlugins();
    }

    void setupScripting()
    {
        auto& scripts = world.scripting();
        ignoreUnused (scripts);
    }

    void setupRepos()
    {
        if (! isFirstRun)
            return;
#if ELEMENT_UPDATER

#endif
    }

    void setupLogging()
    {
        Logger::setCurrentLogger (&world.logger());
    }
};

//=============================================================================
#if JUCE_LINUX
Application::MidiSettingsApply::MidiSettingsApply (Context& c)
    : context (c)
{
    juce::WeakReference<Application::MidiSettingsApply> safeThis (this);
    connection = juce::MidiDeviceListConnection::make ([safeThis] {
        if (auto self = safeThis.get())
        {
            self->connection.reset();
            self->context.midi().applySettings (
                self->context.settings());
        }
    });
}
#endif

Application::Application()
{
}

Application::~Application() {}

const String Application::getApplicationName() { return Util::appName(); }

const String Application::getApplicationVersion() { return ELEMENT_VERSION_STRING; }

bool Application::moreThanOneInstanceAllowed() { return true; }

bool Application::acquireProfileInstanceLock()
{
    profileInstanceLock = std::make_unique<InterProcessLock> (
        "elementumProfileLock_" + String::toHexString (profileIdentifier.hashCode64()));

    // Register before acquiring the lock so a just-started instance cannot miss
    // a duplicate-launch notification in the small interval after it acquires it.
    MessageManager::getInstance()->registerBroadcastListener (this);
    profileBroadcastListenerRegistered = true;

    if (profileInstanceLock->enter (0))
        return true;

    MessageManager::broadcastMessage (encodeProfileInstanceMessage (profileIdentifier, launchArguments));
    releaseProfileInstanceLock();
    return false;
}

void Application::releaseProfileInstanceLock()
{
    if (profileBroadcastListenerRegistered)
    {
        MessageManager::getInstance()->deregisterBroadcastListener (this);
        profileBroadcastListenerRegistered = false;
    }

    profileInstanceLock.reset();
}

void Application::restoreMainWindow()
{
    if (! world)
        return;

    if (auto* const gui = world->services().find<GuiService>())
    {
        if (auto* const window = gui->getMainWindow())
        {
            if (! window->isOnDesktop())
                window->addToDesktop();

            window->setMinimised (false);
            window->setVisible (true);

            // Temporarily raise the window above other windows so a duplicate
            // launch also restores an existing but inactive instance.
            const bool wasAlwaysOnTop = window->isAlwaysOnTop();
            if (! wasAlwaysOnTop)
                window->setAlwaysOnTop (true);
            window->toFront (true);
            if (! wasAlwaysOnTop)
            {
                window->setAlwaysOnTop (false);
                window->toFront (true);
            }

            revealMainWindowWhenReady = false;
            return;
        }
    }

    revealMainWindowWhenReady = true;
}

void Application::maybeOpenCommandLineArguments (const StringArray& arguments)
{
    for (const auto& argument : arguments)
    {
        const auto value = argument.unquoted().trim();
        if (value.startsWith ("elementum://"))
        {
            handleURLSchemeCallback (value);
            continue;
        }

        const File sessionFile = File::isAbsolutePath (value)
                                     ? File (value)
                                     : File::getCurrentWorkingDirectory().getChildFile (value);
        if (! sessionFile.existsAsFile())
            continue;

        if (auto* const sc = world->services().find<SessionService>())
        {
            if (sessionFile.hasFileExtension ("els"))
                sc->openFile (sessionFile);
            else if (sessionFile.hasFileExtension ("elg"))
                sc->importGraph (sessionFile);
        }
    }
}

void Application::handleProfileInstanceMessage (const String& message)
{
    String messageProfile;
    StringArray arguments;
    if (! decodeProfileInstanceMessage (message, messageProfile, arguments)
        || messageProfile != profileIdentifier)
        return;

    restoreMainWindow();
    const auto* const gui = world ? world->services().find<GuiService>() : nullptr;
    if (gui != nullptr && gui->getMainWindow() != nullptr)
        maybeOpenCommandLineArguments (arguments);
    else
        pendingProfileArguments.addArray (arguments);
}

void Application::initialise (const String& commandLine)
{
    launchCommandLine = commandLine;

    const auto pluginDetailsOptions = parsePluginDetailsLaunchArguments (commandLine);
    if (pluginDetailsOptions.isRequested())
    {
        initializeModulePath();
        pluginDetailsExportThread = std::make_unique<PluginDetailsExportThread> (*this, pluginDetailsOptions);
        pluginDetailsExportThread->startThread();
        return;
    }

    const bool isScannerWorker = commandLine.contains (EL_PLUGIN_SCANNER_PROCESS_ID);
    if (isScannerWorker)
    {
        if (! maybeLaunchScannerWorker (commandLine))
            quit();
        return;
    }

    const auto parsed = parseLaunchArguments (commandLine);
    launchArguments = parsed.remaining;
    if (parsed.profileDirectoryError.isNotEmpty() && ! isScannerWorker)
    {
        AlertWindow::showMessageBox (AlertWindow::WarningIcon,
                                     "Profile Directory Unavailable",
                                     "The requested profile could not be used. Elementum will use the default profile instead.\n\n"
                                         + parsed.profileDirectoryError);
    }

    const auto applicationDataDirectory = parsed.applicationDataDirectory;
    if (applicationDataDirectory.isDirectory())
        DataPath::setApplicationDataDirOverride (applicationDataDirectory);

    profileIdentifier = normaliseProfileIdentifier (applicationDataDirectory.isDirectory()
                                                        ? applicationDataDirectory
                                                        : DataPath::applicationDataDir());
    if (! isScannerWorker && ! acquireProfileInstanceLock())
    {
        quit();
        return;
    }

    world = std::make_unique<Context> (RunMode::Standalone, commandLine, applicationDataDirectory);
    initializeModulePath();
    printCopyNotice();

#if JUCE_MAC || JUCE_WINDOWS
    registerURLSchemeHandler();
#endif

    launchApplication();
}

void Application::actionListenerCallback (const String& message)
{
    if (message == "finishedLaunching")
    {
        finishLaunching();
        return;
    }

    handleProfileInstanceMessage (message);
}

bool Application::canShutdown()
{
    if (! MessageManager::getInstance()->isThisTheMessageThread())
    {
        auto result = MessageManager::getInstance()->callSync (Application::canShutdown);
        return result.has_value() ? *result : true;
    }

    if (auto app = dynamic_cast<Application*> (getInstance()))
    {
        auto& services = app->world->services();
        auto ssvc = services.find<SessionService>();
        return ! ssvc->hasSessionChanged();
    }

    return true;
}

void Application::shutdown()
{
    if (! world)
    {
        releaseProfileInstanceLock();
        return;
    }

#if JUCE_MAC || JUCE_WINDOWS
    unregisterURLSchemeHandler();
#endif

#if JUCE_LINUX
    applyMidiSettings.reset();
#endif
    workers.clearQuick (true);
    auto& srvs = world->services();
    srvs.saveSettings();

    if (authStartupThread && authStartupThread->isThreadRunning())
        authStartupThread->stopThread (3000);
    authStartupThread.reset();

    auto& devices (world->devices());
    devices.closeAudioDevice();

    auto engine (world->audio());
    auto& plugins (world->plugins());
    auto& settings (world->settings());
    auto& midi (world->midi());
    auto* props = settings.getUserSettings();
    plugins.stopScanningAudioPlugins(); // the scan thread reads the properties file
    plugins.setPropertiesFile (nullptr); // must be done before Settings is deleted

    srvs.deactivate();
    srvs.shutdown();
    sCanShutdown.store (1, std::memory_order_release);

    plugins.saveUserPlugins (settings);
    midi.writeSettings (settings);

    if (auto el = world->devices().createStateXml())
        props->setValue (Settings::devicesKey, el.get());

    engine = nullptr;
    Logger::setCurrentLogger (nullptr);
    world->setEngine (nullptr);
    world = nullptr;
    releaseProfileInstanceLock();
}

void Application::systemRequestedQuit()
{
    if (! world)
    {
        Application::quit();
        return;
    }

    auto* sc = world->services().find<SessionService>();

    if (world->settings().askToSaveSession())
    {
        // - 0 if the third button was pressed ('cancel')
        // - 1 if the first button was pressed ('yes')
        // - 2 if the middle button was pressed ('no')
        const int res = ! sc->hasSessionChanged() ? 2
                                                  : AlertWindow::showYesNoCancelBox (AlertWindow::NoIcon, "Save Session", "This session may have changes. Would you like to save before exiting?");

        if (res == 1)
            sc->saveSession();
        if (res != 0)
            Application::quit();
    }
    else
    {
        if (sc->getSessionFile().existsAsFile())
        {
            sc->saveSession (false, false, false);
        }
        else
        {
            if (AlertWindow::showOkCancelBox (AlertWindow::NoIcon, "Save Session", "This session has not been saved to disk yet.\nWould you like to before exiting?", "Yes", "No"))
            {
                sc->saveSession();
            }
        }

        Application::quit();
    }
}

void Application::maybeOpenCommandLineFile (const String& commandLine)
{
    maybeOpenCommandLineArguments (parseLaunchArguments (commandLine).remaining);
}

void Application::anotherInstanceStarted (const String& commandLine)
{
    if (world)
        maybeOpenCommandLineFile (commandLine);
}

void Application::handleURLSchemeCallback (const String& urlString)
{
    const URL url (urlString);
    if (url.getDomain() == "auth" && url.getSubPath() == "callback")
    {
        if (auto* gui = world->services().find<GuiService>())
            if (auto* mainWindow = gui->getMainWindow())
                mainWindow->toFront (true);

        auth::handleCallback (urlString, world->settings());
    }
}

void Application::suspended() {}

void Application::resumed()
{
    auto& devices (world->devices());
    devices.restartLastAudioDevice();
}

void Application::finishLaunching()
{
    if (nullptr == startup)
        return;

    if (world->settings().scanForPluginsOnStartup())
        world->plugins().scanAudioPlugins();

    const bool isFirstRun = startup->isFirstRun;
    startup.reset();

    auto& ui = *world->services().find<UI>();
    if (auto cf = createContentFactory())
        ui.setContentFactory (std::move (cf));

    world->services().run();

    // Attempt to restore auth state from a stored refresh token in the background.
    authStartupThread = std::make_unique<AuthStartupThread> (*world);
    authStartupThread->startThread (Thread::Priority::low);

    if (! isFirstRun && world->settings().checkForUpdates())
        startTimer (10 * 1000);

    maybeOpenCommandLineArguments (launchArguments);

    if (revealMainWindowWhenReady)
        restoreMainWindow();

    if (! pendingProfileArguments.isEmpty())
    {
        const auto arguments = std::move (pendingProfileArguments);
        pendingProfileArguments.clear();
        maybeOpenCommandLineArguments (arguments);
    }
}

void Application::AuthStartupThread::run()
{
    auth::maybeRefreshOnStartup (ctx.settings());

    // Re-apply the channel preference to the updater from the message thread:
    // the startup refresh may have fetched a new signed appcast URL.
    juce::MessageManager::callAsync ([&ctx = ctx]() {
        if (auto* gui = ctx.services().find<GuiService>())
            gui->applyStoredChannelToUpdater();
    });
}

void Application::finishPluginDetailsExport (Result result)
{
    if (pluginDetailsExportThread != nullptr)
    {
        pluginDetailsExportThread->waitForThreadToExit (1000);
        pluginDetailsExportThread.reset();
    }

    if (result.failed() && ! isPluginDetailsScanFailure (result))
        AlertWindow::showMessageBox (AlertWindow::WarningIcon, "Plugin Details Export Failed", result.getErrorMessage());

    quit();
}

void Application::printCopyNotice()
{
    String appName = Util::appName();
    appName << " v" << getApplicationVersion() << " (GPL v3)";
    Logger::writeToLog (appName);
    Logger::writeToLog ("Elementum is a Fooboody Studio fork of Element.");
    Logger::writeToLog (String ("Original work Copyright (c) 2017-%YEAR% Kushview, LLC.\n")
                            .replace ("%YEAR%", String (Time::getCurrentTime().getYear())));
    Logger::writeToLog ("Fork modifications Copyright (c) 2026 Fooboody Studio.");
}

bool Application::maybeLaunchScannerWorker (const String& commandLine)
{
    workers.clearQuick (true);
    PluginManager pluginManager;
    workers.add (pluginManager.createAudioPluginScannerWorker());
    StringArray processIds = { EL_PLUGIN_SCANNER_PROCESS_ID };
    for (auto* worker : workers)
    {
        for (const auto& pid : processIds)
        {
            if (worker->initialiseFromCommandLine (commandLine, pid, 20 * 1000))
            {
#if JUCE_MAC
                Process::setDockIconVisible (false);
#endif
                return true;
            }
        }
    }

    return false;
}

void Application::launchApplication()
{
    if (startup != nullptr)
        return;

    startup = std::make_unique<Startup> (*world);
    startup->addActionListener (this);
    startup->launchApplication();
#if JUCE_LINUX
    applyMidiSettings = std::make_unique<MidiSettingsApply> (*world);
#endif
}

void Application::initializeModulePath()
{
    const File path (File::getSpecialLocation (File::invokedExecutableFile));
    File modDir = path.getParentDirectory().getParentDirectory().getChildFile ("lib/elementum").getFullPathName();
#if JUCE_DEBUG
    if (! modDir.exists())
    {
        modDir = path.getParentDirectory().getParentDirectory().getChildFile ("modules");
    }
#endif

#if JUCE_WINDOWS
    String putEnv = "ELEMENTUM_MODULE_PATH=";
    putEnv << modDir.getFullPathName();
    putenv (putEnv.toRawUTF8());
#else
    setenv ("ELEMENTUM_MODULE_PATH", modDir.getFullPathName().toRawUTF8(), 1);
#endif
}

void Application::timerCallback()
{
    world->services().find<UI>()->checkUpdates (true);
    stopTimer();
}

} // namespace element
