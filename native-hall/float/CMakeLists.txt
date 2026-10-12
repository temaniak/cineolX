# Independent experimental engine/check; original processing targets are unchanged.
add_executable(cineol_float_hall_check check.cpp)
target_link_libraries(cineol_float_hall_check PRIVATE native_hall_core)
set_target_properties(cineol_float_hall_check PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
option(CINEOL_BUILD_FLOAT_AB "Build separate experimental Hall A/B plugin" OFF)
if(CINEOL_BUILD_FLOAT_AB)
    if(NOT CINEOL_BUILD_PLUGIN)
        message(FATAL_ERROR "Float A/B plugin requires CINEOL_BUILD_PLUGIN=ON for JUCE")
    endif()
    juce_add_plugin(CineolFloatHallAB VERSION ${PROJECT_VERSION} PRODUCT_NAME "Cineol Float Hall AB"
        COMPANY_NAME "Reflexion224" BUNDLE_ID "net.joelanders.cineolfloatab"
        PLUGIN_MANUFACTURER_CODE Rflx PLUGIN_CODE CfAB
        FORMATS ${_formats} IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE AU_MAIN_TYPE kAudioUnitType_Effect
        VST3_CATEGORIES Fx Reverb COPY_PLUGIN_AFTER_BUILD FALSE
        MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "Cineol Float Hall AB processes your audio input.")
    target_sources(CineolFloatHallAB PRIVATE plugin/Processor.cpp plugin/Editor.cpp ../plugin/RomBank.cpp)
    target_link_libraries(CineolFloatHallAB PRIVATE native_hall_core native_224_import native_xl_import
        juce::juce_audio_utils juce::juce_recommended_config_flags)
    target_compile_definitions(CineolFloatHallAB PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_STANDALONE_PLUGIN_AUTO_OPEN_AUDIO_INPUT=1)
    if(APPLE)
        lexplug_sign_bundles(CineolFloatHallAB)
    endif()
    juce_add_console_app(cineol_float_ab_plugin_check PRODUCT_NAME "cineol_float_ab_plugin_check")
    target_sources(cineol_float_ab_plugin_check PRIVATE plugin_check.cpp)
    target_link_libraries(cineol_float_ab_plugin_check PRIVATE CineolFloatHallAB juce::juce_audio_utils)
endif()
