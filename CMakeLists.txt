# SPDX-FileCopyrightText: 2026 Kushview, LLC
# SPDX-FileCopyrightText: 2026 Fooboody Studio
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.26.0)
project(elementum VERSION 1.2.0)
# Keep PROJECT_VERSION synchronized with upstream; this suffix identifies the fork revision.
set(PROJECT_VERSION_SUFFIX "-fooboody.1")
set(PROJECT_VERSION_FULL "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include(cmake/Element.cmake)
if(ELEMENT_ENABLE_UPDATER)
    if(APPLE)
        include(cmake/FindSparkle.cmake)
    elseif(WIN32)
        include(cmake/FindWinSparkle.cmake)
    endif()
endif()
include(cmake/FindJack.cmake)
include(cmake/FindSol2.cmake)
include(cmake/FindJUCE.cmake)

# Global JUCE properties
set_directory_properties(PROPERTIES JUCE_COMPANY_NAME "Fooboody Studio")
set_directory_properties(PROPERTIES JUCE_COMPANY_COPYRIGHT "Original work © Kushview, LLC; fork modifications © Fooboody Studio")
set_directory_properties(PROPERTIES JUCE_COMPANY_WEBSITE "https://github.com/fooboody/elementum")
# No dedicated support mailbox has been published for this fork.
set_directory_properties(PROPERTIES JUCE_COMPANY_EMAIL "")

# CLAP Setup
set(CMAKE_POLICY_DEFAULT_CMP0177 OLD)
add_subdirectory(deps/clap-juce-extensions EXCLUDE_FROM_ALL)

# BOOST Setup
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30.0")
    cmake_policy(SET CMP0167 OLD)
endif()

find_package(Boost 1.74.0)
if(NOT Boost_FOUND)
    message(" -- Checking user SDK's for boost")
    set(Boost_ROOT "${USER_HOME_DIRECTORY}/SDKs/include")
    find_package(Boost 1.74.0 REQUIRED)
endif()

add_subdirectory(data)
add_subdirectory(scripts)
add_subdirectory(include/element)
add_subdirectory(src/el)
add_subdirectory(src)

## Tests
if (ELEMENT_BUILD_TESTS)
    ## Unit tests
    enable_testing()
    add_subdirectory(test)
endif()

## Standalone Application
if(NOT APPLE)
    # For backward compatibility with the old meson build, not-Mac installs 
    # expect the exe name to be all lower case.
    set(ELEMENT_STANDALONE_PRODUCT_NAME "elementum")
else()
    set(ELEMENT_STANDALONE_PRODUCT_NAME "Elementum")
endif()

juce_add_gui_app(element_app
    PRODUCT_NAME            "${ELEMENT_STANDALONE_PRODUCT_NAME}"
    # JUCE requires BUILD_VERSION to be major.minor.bugfix; use PROJECT_VERSION_FULL only for display and packaging.
    BUILD_VERSION           "${PROJECT_VERSION}"
    DOCUMENT_EXTENSIONS     els elg
    ICON_BIG                data/images/icon.png
    ICON_SMALL              data/images/icon.png

    BUNDLE_ID               "io.github.fooboody.elementum"
    MICROPHONE_PERMISSION_ENABLED TRUE

    PLIST_TO_MERGE          "${ELEMENT_APP_PLIST_TO_MERGE}"
)

target_sources(element_app PRIVATE src/main.cc)
target_link_libraries(element_app PRIVATE kv::element)
set_target_properties(element_app PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(APPLE)
    add_custom_target(uninstaller
        COMMAND osacompile -o "${PROJECT_BINARY_DIR}/Uninstall.app" "${PROJECT_SOURCE_DIR}/util/uninstall.applescript"
        COMMENT "Building Uninstall.app from AppleScript"
        VERBATIM
    )
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${PROJECT_BINARY_DIR}/Uninstall.app")

    if(ELEMENT_ENABLE_UPDATER)
        target_compile_options(element_app PRIVATE "-F${sparkle_SOURCE_DIR}")
        target_link_options(element_app PRIVATE "-F${sparkle_SOURCE_DIR}" -framework Sparkle)
        target_sources(element_app PRIVATE src/sparkle.mm)
        add_custom_command(TARGET element_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf
                    "$<TARGET_BUNDLE_CONTENT_DIR:element_app>/Frameworks/Sparkle.framework"
            COMMAND /usr/bin/ditto
                    "${SPARKLE_FRAMEWORK}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:element_app>/Frameworks/Sparkle.framework")
        # Set rpath to find Sparkle.framework in app bundle
        set_target_properties(element_app PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    endif()
    install(TARGETS element_app
        BUNDLE DESTINATION "Applications"
        COMPONENT "Standalone")
elseif(WIN32)
    # MSVC 14.44+ introduced a constexpr std::mutex constructor that zero-initializes
    # _Thread_id to -1. In debug builds, _Mtx_lock's deadlock detection can misfire
    # when JUCE acquires a mutex inside a noexcept function (MessageManager::isThisTheMessageThread),
    # causing std::terminate(). This define restores the old _Mtx_init_in_situ path.
    target_compile_definitions(element_app PRIVATE
        $<$<CONFIG:Debug>:_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR>)
    if(ELEMENT_ENABLE_UPDATER)
        target_link_libraries(element_app PRIVATE WinSparkle::WinSparkle)
        target_sources(element_app PRIVATE src/winsparkle.cc)
        set_source_files_properties(src/winsparkle.cc PROPERTIES 
            COMPILE_DEFINITIONS "ELEMENT_APPCAST_URL=\"${WIN_SPARKLE_FEED_URL}\";ELEMENT_EDDSA_PUBLIC_KEY=\"${WIN_SPARKLE_PUBLIC_KEY}\"")
        add_custom_command(TARGET element_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${WIN_SPARKLE_DLL}" "$<TARGET_FILE_DIR:element_app>")
        install(FILES "${WIN_SPARKLE_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(TARGETS element_app DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    install(TARGETS element_app DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(ELEMENT_BUILD_PLUGINS)
    set(ELEMENT_PLUGIN_FORMATS "AU;LV2;VST3")
    if(ELEMENT_ENABLE_VST2)
        list(APPEND ELEMENT_PLUGIN_FORMATS "VST")
    endif()

    ## Instrument Plugin
    juce_add_plugin(element_instrument
        PLUGIN_NAME                 "Elementum"
        PRODUCT_NAME                "Elementum"
        # JUCE requires BUILD_VERSION to be major.minor.bugfix.
        BUILD_VERSION               "${PROJECT_VERSION}"
        PLUGIN_MANUFACTURER_CODE    Foob
        PLUGIN_CODE                 Elum

        BUNDLE_ID                   "io.github.fooboody.elementum.plugin"
        AU_MAIN_TYPE                kAudioUnitType_MusicDevice

        LV2URI                      "https://github.com/fooboody/elementum/plugins/elementum"

        IS_SYNTH                ON
        NEEDS_MIDI_INPUT        ON
        NEEDS_MIDI_OUTPUT       OFF
        EDITOR_WANTS_KEYBOARD_FOCUS ON
        FORMATS                 ${ELEMENT_PLUGIN_FORMATS}
    )
    clap_juce_extensions_plugin(TARGET element_instrument
        CLAP_ID "io.github.fooboody.elementum"
        CLAP_FEATURES instrument)
    target_sources(element_instrument PRIVATE src/plugins/instrument.cc)
    target_link_libraries(element_instrument PRIVATE kv::element)
    element_setup_plugin(element_instrument)
    element_install_plugin(element_instrument)
    
    ## Effect Plugin
    juce_add_plugin(element_effect
        PLUGIN_NAME                 "Elementum FX"
        PRODUCT_NAME                "Elementum FX"
        # JUCE requires BUILD_VERSION to be major.minor.bugfix.
        BUILD_VERSION               "${PROJECT_VERSION}"
        PLUGIN_MANUFACTURER_CODE    Foob
        PLUGIN_CODE                 EuFX

        BUNDLE_ID                   "io.github.fooboody.elementum.fx"
        AU_MAIN_TYPE                kAudioUnitType_Effect

        LV2URI                      "https://github.com/fooboody/elementum/plugins/elementum-fx"
        
        IS_SYNTH                    OFF
        NEEDS_MIDI_INPUT            ON
        NEEDS_MIDI_OUTPUT           OFF
        EDITOR_WANTS_KEYBOARD_FOCUS ON
        FORMATS                     ${ELEMENT_PLUGIN_FORMATS}
    )
    clap_juce_extensions_plugin(TARGET element_effect
        CLAP_ID "io.github.fooboody.elementum-fx"
        CLAP_FEATURES audio-effect)

    target_sources(element_effect PRIVATE src/plugins/effect.cc)
    target_link_libraries(element_effect PRIVATE kv::element)
    element_setup_plugin(element_effect)
    element_install_plugin(element_effect)

    if(APPLE)
        ## MIDI Effect Plugin
        juce_add_plugin(element_midi_effect
            PLUGIN_NAME                 "Elementum MFX"
            PRODUCT_NAME                "Elementum MFX"
            # JUCE requires BUILD_VERSION to be major.minor.bugfix.
            BUILD_VERSION               "${PROJECT_VERSION}"
            PLUGIN_MANUFACTURER_CODE    Foob
            PLUGIN_CODE                 EuMX
            
            BUNDLE_ID                   "io.github.fooboody.elementum.mfx"
            AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor

            IS_SYNTH                    OFF
            NEEDS_MIDI_INPUT            ON
            NEEDS_MIDI_OUTPUT           ON
            EDITOR_WANTS_KEYBOARD_FOCUS ON
            FORMATS                     AU
        )

        target_sources(element_midi_effect PRIVATE src/plugins/midieffect.cc)
        target_link_libraries(element_midi_effect PRIVATE kv::element)
        element_setup_plugin(element_midi_effect)
        
        # MFX version is AU only so function helper won't work here.
        install(TARGETS element_midi_effect_AU
            LIBRARY DESTINATION "Library/Audio/Plug-Ins/Components"
            COMPONENT "Elementum MFX")
    endif()
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "Elementum")
set(CPACK_PACKAGE_VENDOR "Fooboody Studio")
set(CPACK_PACKAGE_CONTACT "https://github.com/fooboody/elementum/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Elementum - Audio Plugin Host and Modular Instrument (Fooboody Studio fork of Element)")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_FULL}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Elementum")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/data/intro.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/GPL-3.0-or-later.txt")

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ELEMENT_PROCESSOR)

if(LINUX)
    set(ELEMENT_SYSTEM_NAME "linux")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "elementum")
    set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2;libjack-jackd2-0;fonts-roboto;libfreetype6;libx11-6;libxext6;libxrandr2;libxinerama1;libxcursor1;libxrender1;libxcomposite1;libcurl4;libgl1;libstdc++6")
elseif(APPLE)
    set(ELEMENT_SYSTEM_NAME "macos")
    set(ELEMENT_PROCESSOR "universal")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
    set(CPACK_SET_DESTDIR OFF)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
else()
    set(ELEMENT_SYSTEM_NAME "windows")
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")
endif()

set(ELEMENT_GENERATOR ${CPACK_GENERATOR})
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${ELEMENT_SYSTEM_NAME}-${ELEMENT_PROCESSOR}")
include(CPack)

# Clean CPack staging directory before and after packaging to prevent macOS relocation issues
add_custom_target(installer
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PROJECT_BINARY_DIR}/_CPack_Packages"
    COMMAND ${CMAKE_COMMAND} --build "${PROJECT_BINARY_DIR}" --target package
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${PROJECT_BINARY_DIR}/_CPack_Packages"
    COMMENT "Cleaning CPack staging and building package"
    VERBATIM
    USES_TERMINAL)
