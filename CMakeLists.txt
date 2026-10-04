cmake_minimum_required(VERSION 3.22)
project(CineolX224 VERSION 0.9.5 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/fp:strict)
    add_compile_definitions(_USE_MATH_DEFINES)
else()
    add_compile_options(-ffp-contract=off)
endif()
option(CINEOL_BUILD_PLUGIN "Build Cineol-X 224 desktop plugin and checks" ON)
option(NATIVE_HALL_BUILD_TOOLS "Build private ROM preparation and DSP checks" ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(CINEOL_BUILD_PLUGIN OR NATIVE_HALL_BUILD_TOOLS)
    set(CINEOL_REFLEXION_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/reflexion" CACHE PATH "Pinned Reflexion checkout")
    set(CINEOL_REFLEXION_OVERLAY "${CMAKE_CURRENT_SOURCE_DIR}/build/deps/reflexion")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/script/prepare_dependency.py"
        reflexion --source "${CINEOL_REFLEXION_SOURCE}" --output "${CINEOL_REFLEXION_OVERLAY}"
        RESULT_VARIABLE _export_result)
    if(NOT _export_result EQUAL 0)
        message(FATAL_ERROR "Initialize the pinned Reflexion dependency before configuring")
    endif()
    include_directories("${CINEOL_REFLEXION_OVERLAY}")
endif()
add_subdirectory(native-hall)
if(NOT CINEOL_BUILD_PLUGIN)
    return()
endif()
if(APPLE)
    enable_language(OBJC OBJCXX)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
    endif()
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
set(_formats VST3 Standalone)
if(APPLE)
    list(PREPEND _formats AU)
endif()
juce_add_binary_data(cineol_ui_data HEADER_NAME CineolUIData.h NAMESPACE CineolUIData
    SOURCES native-hall/plugin/assets/panel.png native-hall/plugin/assets/fadercap.png
        native-hall/plugin/assets/panel-monolithic.png native-hall/plugin/assets/menu-metal.png
        native-hall/plugin/assets/quick-key.png native-hall/plugin/assets/panel-quick-presets.png)
juce_add_plugin(NativeHall224 VERSION ${PROJECT_VERSION} PRODUCT_NAME "Cineol-X 224" COMPANY_NAME "Reflexion224"
    BUNDLE_ID "net.joelanders.nativehall224" PLUGIN_MANUFACTURER_CODE Rflx PLUGIN_CODE Nh24
    FORMATS ${_formats} IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Reverb MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Cineol-X 224 processes your audio input." COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(NativeHall224 PRIVATE native-hall/plugin/Processor.cpp
    native-hall/plugin/Editor.cpp native-hall/plugin/RomBank.cpp native-hall/plugin/Presets.cpp)
target_link_libraries(NativeHall224 PRIVATE native_hall_core native_224_import native_xl_import cineol_ui_data
    juce::juce_audio_utils juce::juce_recommended_config_flags)
target_compile_definitions(NativeHall224 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_STANDALONE_PLUGIN_AUTO_OPEN_AUDIO_INPUT=1)
if(APPLE)
    include("${CINEOL_REFLEXION_SOURCE}/juce-plugin/scripts/sign_bundle.cmake")
    lexplug_sign_bundles(NativeHall224)
endif()
juce_add_console_app(native_hall_plugin_check PRODUCT_NAME "native_hall_plugin_check")
target_sources(native_hall_plugin_check PRIVATE native-hall/tests/plugin_check.cpp)
target_link_libraries(native_hall_plugin_check PRIVATE NativeHall224 juce::juce_audio_utils)
juce_add_console_app(cineol_rom_import_check PRODUCT_NAME "cineol_rom_import_check")
target_sources(cineol_rom_import_check PRIVATE native-hall/tests/rom_import_check.cpp)
target_link_libraries(cineol_rom_import_check PRIVATE NativeHall224 juce::juce_audio_utils)
