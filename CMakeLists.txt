cmake_minimum_required(VERSION 3.22)

project(MidiPatternLauncher VERSION 1.12.3 LANGUAGES C CXX)

# =============================================================================
# JUCE location
# =============================================================================
# Local JUCE checkout. Keep this path local for now.
# Later we can replace this with FetchContent, a Git submodule, or JUCE_DIR cache var.
set(JUCE_DIR "C:/JUCE/JUCE")

add_subdirectory("${JUCE_DIR}" JUCE)

# =============================================================================
# Plugin target
# =============================================================================
juce_add_plugin(MidiPatternLauncher
    COMPANY_NAME "John Pascu"
    BUNDLE_ID "com.johnpascu.midipatternlauncher"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE

    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD TRUE

    PLUGIN_MANUFACTURER_CODE Jpsc
    PLUGIN_CODE Mplg

    FORMATS
        Standalone
        VST3

    PRODUCT_NAME "MIDI Pattern Launcher"
)

# Generate compatibility JuceHeader.h for existing Projucer-style source files.
juce_generate_juce_header(MidiPatternLauncher)

# =============================================================================
# Source files
# =============================================================================
target_sources(MidiPatternLauncher
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ComposerBridgeIpcClient.cpp
)

target_include_directories(MidiPatternLauncher
    PRIVATE
        Source
)

# =============================================================================
# C++ standard
# =============================================================================
target_compile_features(MidiPatternLauncher
    PRIVATE
        cxx_std_17
)

# =============================================================================
# Compile definitions
# =============================================================================
target_compile_definitions(MidiPatternLauncher
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# =============================================================================
# JUCE modules
# =============================================================================
target_link_libraries(MidiPatternLauncher
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
