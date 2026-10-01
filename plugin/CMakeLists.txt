FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW ON)

# ARA 2 lets the plug-in read the track's events directly (Cubase, Studio One, Logic, Reaper ...)
FetchContent_Declare(ARA_SDK
    GIT_REPOSITORY https://github.com/Celemony/ARA_SDK.git
    GIT_TAG releases/2.2.0
    GIT_SHALLOW ON
    GIT_SUBMODULES ARA_API ARA_Library)

FetchContent_MakeAvailable(JUCE)
FetchContent_GetProperties(ARA_SDK)
if(NOT ara_sdk_POPULATED)
    FetchContent_Populate(ARA_SDK)
endif()
juce_set_ara_sdk_path(${ara_sdk_SOURCE_DIR})

set(AMT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AMT_FORMATS AU)
endif()

juce_add_plugin(AlignMyTime
    PRODUCT_NAME "Align my Time"
    COMPANY_NAME "Align my Time"
    BUNDLE_ID "com.alignmytime.plugin"
    PLUGIN_MANUFACTURER_CODE Amyt
    PLUGIN_CODE Amt1
    FORMATS ${AMT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    IS_ARA_EFFECT TRUE
    ARA_FACTORY_ID "\"com.alignmytime.plugin.arafactory\""
    ARA_DOCUMENT_ARCHIVE_ID "\"com.alignmytime.plugin.aradocumentarchive.1\""
    ARA_CONTENT_TYPES 0
    ARA_TRANSFORMATION_FLAGS 0
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AlignMyTime PRIVATE
    Source/AlignSession.cpp
    Source/DocumentController.cpp
    Source/Exporter.cpp
    Source/Localisation.cpp
    Source/PlaybackRenderer.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/PreviewPlayer.cpp
    Source/SourceLoader.cpp
    Source/Translations.cpp
    Source/ui/Header.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/RenderPage.cpp
    Source/ui/ReviewPage.cpp
    Source/ui/SettingsPanel.cpp
    Source/ui/TapPage.cpp
    Source/ui/TempoEditor.cpp
    Source/ui/TempoLane.cpp
    Source/ui/WaveformView.cpp)

target_include_directories(AlignMyTime PRIVATE Source)

if(MSVC)
    target_compile_options(AlignMyTime PRIVATE /utf-8)
endif()

target_compile_definitions(AlignMyTime PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(AlignMyTime
    PRIVATE
        amt_core
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# End-to-end test without a DAW (also writes screenshots when given a folder)
if(AMT_BUILD_TESTS)
    # The ARA library is normally compiled into each plug-in format; the test needs it too.
    add_executable(AlignMyTimeSmokeTest tests/PluginSmokeTest.cpp
        ${juce_SOURCE_DIR}/modules/juce_audio_plugin_client/juce_audio_plugin_client_ARA.cpp)
    target_include_directories(AlignMyTimeSmokeTest PRIVATE Source $<TARGET_PROPERTY:AlignMyTime,INCLUDE_DIRECTORIES>)
    target_compile_definitions(AlignMyTimeSmokeTest PRIVATE $<TARGET_PROPERTY:AlignMyTime,COMPILE_DEFINITIONS>)
    target_link_libraries(AlignMyTimeSmokeTest PRIVATE AlignMyTime amt_core)
    add_test(NAME plugin_smoke_test COMMAND AlignMyTimeSmokeTest)
endif()
