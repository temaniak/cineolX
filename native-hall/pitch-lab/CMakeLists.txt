option(CINEOL_BUILD_PITCH_LAB "Build independent spectral pitch and Hall test bench" OFF)
if(NOT CINEOL_BUILD_PITCH_LAB)
    return()
endif()
cmake_minimum_required(VERSION 3.24)
include(FetchContent)
FetchContent_Declare(pitch_signalsmith_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 7bb1c8f36794d6980080ce817c91f8da8fe56a66)
FetchContent_MakeAvailable(pitch_signalsmith_linear)
FetchContent_Declare(pitch_signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da
    GIT_SUBMODULES "")
FetchContent_MakeAvailable(pitch_signalsmith_stretch)
include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/PrecisionEngine.cmake")
add_library(cineol_pitch_lab_core STATIC core.cpp signalsmith_reference.cpp)
target_link_libraries(cineol_pitch_lab_core PUBLIC native_hall_core cineol_clean_core signalsmith-stretch)
set_target_properties(cineol_pitch_lab_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_executable(cineol_pitch_lab_core_check check.cpp)
target_link_libraries(cineol_pitch_lab_core_check PRIVATE cineol_pitch_lab_core)
if(NOT CINEOL_BUILD_PLUGIN)
    return()
endif()
juce_add_plugin(CineolPitchLab VERSION ${PROJECT_VERSION} PRODUCT_NAME "Cineol Pitch Lab"
    COMPANY_NAME "NK Stuff" BUNDLE_ID "net.nkstuff.cineolpitchlab"
    PLUGIN_MANUFACTURER_CODE Rflx PLUGIN_CODE CpLB
    FORMATS ${_formats} IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Pitch\ Shift Reverb COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "Cineol Pitch Lab processes your audio input.")
target_sources(CineolPitchLab PRIVATE plugin/Processor.cpp plugin/Editor.cpp)
target_link_libraries(CineolPitchLab PRIVATE cineol_pitch_lab_core juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
target_compile_definitions(CineolPitchLab PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_STANDALONE_PLUGIN_AUTO_OPEN_AUDIO_INPUT=1)
if(APPLE)
    foreach(_format AU VST3)
        string(TOLOWER "${_format}" _lower)
        if(_format STREQUAL "AU")
            set(_exports "${CMAKE_CURRENT_SOURCE_DIR}/macos-au.exports")
        else()
            set(_exports "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/macos-vst3.exports")
        endif()
        target_link_options(CineolPitchLab_${_format} PRIVATE "LINKER:-exported_symbols_list,${_exports}")
        set_property(TARGET CineolPitchLab_${_format} APPEND PROPERTY LINK_DEPENDS "${_exports}")
    endforeach()
    foreach(_format AU VST3 Standalone)
        add_custom_command(TARGET CineolPitchLab_${_format} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:CineolPitchLab_${_format}>/Resources/Licenses"
            COMMAND ${CMAKE_COMMAND} -E copy "${pitch_signalsmith_stretch_SOURCE_DIR}/LICENSE.txt" "$<TARGET_BUNDLE_CONTENT_DIR:CineolPitchLab_${_format}>/Resources/Licenses/Signalsmith-Stretch.txt"
            COMMAND ${CMAKE_COMMAND} -E copy "${pitch_signalsmith_linear_SOURCE_DIR}/LICENSE.txt" "$<TARGET_BUNDLE_CONTENT_DIR:CineolPitchLab_${_format}>/Resources/Licenses/Signalsmith-Linear.txt")
    endforeach()
    lexplug_sign_bundles(CineolPitchLab)
endif()
juce_add_console_app(cineol_pitch_lab_plugin_check PRODUCT_NAME "cineol_pitch_lab_plugin_check")
target_sources(cineol_pitch_lab_plugin_check PRIVATE plugin_check.cpp)
target_link_libraries(cineol_pitch_lab_plugin_check PRIVATE CineolPitchLab juce::juce_audio_utils)
