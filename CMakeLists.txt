cmake_minimum_required(VERSION 3.22)

project(TheAmazingNotta VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(NOTTA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NOTTA_FORMATS AU)
endif()

juce_add_plugin(TheAmazingNotta
    COMPANY_NAME "Notta Audio"
    BUNDLE_ID com.nottaudio.theamazingnotta
    PLUGIN_MANUFACTURER_CODE Ntta
    PLUGIN_CODE TAn1
    PRODUCT_NAME "The Amazing Notta"
    DESCRIPTION "A labyrinthine, visual-only mastering placebo"
    FORMATS ${NOTTA_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx)

juce_add_binary_data(TheAmazingNottaAssets
    SOURCES
        Images/Knob001.png
        Images/BOXBAPLogo.png
        Images/SCREWYOU-BUTTON.png
        Images/OffOnToggleALLA-OFF.png
        Images/OffOnToggleALLA-ON.png
        Images/Reel2Reel/Reel2Reel-All.png
        Images/Reel2Reel/LEFT-REEL.png
        Images/Reel2Reel/RIGHT-REEL.png
        "Images/Backgrounds/READY TO USE/BACKGROUND-COLOR.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-DRIP.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-HEAT.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-IDK.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-JUICE.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-MAGIC.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-MOJO.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-SAUCE.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-SOMETHIN.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-VIBE.png"
        "Images/Backgrounds/READY TO USE/BACKGROUND-WARMTH.png"
        "Images/TUBES/READY TO USE/TUBE-LEFT.png"
        "Images/TUBES/READY TO USE/TUBE-RIGHT.png")

juce_generate_juce_header(TheAmazingNotta)

target_sources(TheAmazingNotta PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(TheAmazingNotta PRIVATE cxx_std_17)
target_compile_definitions(TheAmazingNotta PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TheAmazingNotta PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    TheAmazingNottaAssets
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

add_custom_target(TheAmazingNotta_AllFormats ALL)
add_dependencies(TheAmazingNotta_AllFormats
    TheAmazingNotta_Standalone
    TheAmazingNotta_VST3)

if(WIN32)
    set(NOTTA_SYSTEM_VST3_BUNDLE "C:/Program Files/Common Files/VST3/The Amazing Notta.vst3")
    add_custom_command(TARGET TheAmazingNotta_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:TheAmazingNotta_VST3>/../.."
            "${NOTTA_SYSTEM_VST3_BUNDLE}"
        COMMENT "Syncing The Amazing Notta VST3 to the system VST3 folder"
        VERBATIM)
endif()