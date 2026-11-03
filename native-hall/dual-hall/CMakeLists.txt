option(CINEOL_BUILD_DUAL_HALL "Build independent dual Hall B crossfeedback prototype" OFF)
if(NOT CINEOL_BUILD_DUAL_HALL)
    return()
endif()
add_executable(cineol_dual_hall_check check.cpp)
target_link_libraries(cineol_dual_hall_check PRIVATE native_hall_core)
if(NOT CINEOL_BUILD_PLUGIN)
    return()
endif()
juce_add_plugin(CineolDualHallB VERSION ${PROJECT_VERSION} PRODUCT_NAME "Cineol Dual Hall B"
    COMPANY_NAME "NK Stuff" BUNDLE_ID "net.nkstuff.cineoldualhallb"
    PLUGIN_MANUFACTURER_CODE Rflx PLUGIN_CODE CdHB
    FORMATS ${_formats} IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb COPY_PLUGIN_AFTER_BUILD FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "Cineol Dual Hall B processes your audio input.")
target_sources(CineolDualHallB PRIVATE plugin/Processor.cpp plugin/Editor.cpp)
target_link_libraries(CineolDualHallB PRIVATE native_hall_core
    juce::juce_audio_utils juce::juce_recommended_config_flags)
target_compile_definitions(CineolDualHallB PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_STANDALONE_PLUGIN_AUTO_OPEN_AUDIO_INPUT=1)
if(APPLE)
    foreach(_format AU VST3)
        string(TOLOWER "${_format}" _lower)
        if(_format STREQUAL "AU")
            set(_exports "${CMAKE_CURRENT_SOURCE_DIR}/macos-au.exports")
        else()
            set(_exports "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/macos-vst3.exports")
        endif()
        target_link_options(CineolDualHallB_${_format} PRIVATE "LINKER:-exported_symbols_list,${_exports}")
        set_property(TARGET CineolDualHallB_${_format} APPEND PROPERTY LINK_DEPENDS "${_exports}")
    endforeach()
    lexplug_sign_bundles(CineolDualHallB)
endif()
juce_add_console_app(cineol_dual_hall_plugin_check PRODUCT_NAME "cineol_dual_hall_plugin_check")
target_sources(cineol_dual_hall_plugin_check PRIVATE plugin_check.cpp)
target_link_libraries(cineol_dual_hall_plugin_check PRIVATE CineolDualHallB juce::juce_audio_utils)
