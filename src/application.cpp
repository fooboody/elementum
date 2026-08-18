// SPDX-FileCopyrightText: Copyright (C) Kushview, LLC.
// SPDX-FileCopyrightText: Copyright (C) 2026 Fooboody Studio
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <element/application.hpp>
#include <element/services.hpp>
#include <element/version.hpp>
#include <element/context.hpp>
#include <element/devices.hpp>
#include <element/plugins.hpp>
#include <element/settings.hpp>
#include <element/ui.hpp>
#include <element/ui/commands.hpp>
#include <element/ui/commands.hpp>
#include <element/ui/content.hpp>
#include <element/ui/mainwindow.hpp>
#include <element/ui/updater.hpp>

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
};

String normaliseProfileIdentifier (const File& directory)
{
    auto identifier = directory.getLinkedTarget().getFullPathName();
#if JUCE_WINDOWS
    identifier = identifier.toLowerCase();
#endif
    return identifier;
}

ParsedLaunchArguments parseLaunchArguments (const String& commandLine)
{
    ParsedLaunchArguments parsed;
    const auto tokens = StringArray::fromTokens (commandLine, true);

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];
        String value;
        bool isDataDirectoryOption = false;

        if (token == "--datadir" && i + 1 < tokens.size())
        {
            value = tokens[++i].unquoted();
            isDataDirectoryOption = true;
        }
        else if (token.startsWith ("--datadir="))
        {
            value = token.fromFirstOccurrenceOf ("=", false, false).unquoted();
            isDataDirectoryOption = value.isNotEmpty();
        }

        if (isDataDirectoryOption)
        {
            parsed.applicationDataDirectory = File();
            const auto directory = File::isAbsolutePath (value)
                                       ? File (value)
                                       : File::getCurrentWorkingDirectory().getChildFile (value);
            const auto settingsFile = Settings::getSettingsFileForDataDirectory (directory);
            if (directory.isDirectory() && settingsFile.existsAsFile())
                parsed.applicationDataDirectory = directory.getLinkedTarget();

            continue;
        }

        parsed.remaining.add (token);
    }

    return parsed;
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
    const auto parsed = parseLaunchArguments (commandLine);
    launchArguments = parsed.remaining;

    const auto applicationDataDirectory = parsed.applicationDataDirectory;
    if (applicationDataDirectory.isDirectory())
        DataPath::setApplicationDataDirOverride (applicationDataDirectory);

    profileIdentifier = normaliseProfileIdentifier (applicationDataDirectory.isDirectory()
                                                        ? applicationDataDirectory
                                                        : DataPath::applicationDataDir());

    const bool isScannerWorker = commandLine.contains (EL_PLUGIN_SCANNER_PROCESS_ID);
    if (! isScannerWorker && ! acquireProfileInstanceLock())
    {
        quit();
        return;
    }

    world = std::make_unique<Context> (RunMode::Standalone, commandLine, applicationDataDirectory);
    if (maybeLaunchScannerWorker (commandLine))
        return;

    if (isScannerWorker && ! acquireProfileInstanceLock())
    {
        quit();
        return;
    }

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
    workers.add (world->plugins().createAudioPluginScannerWorker());
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
