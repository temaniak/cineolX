# Shared desktop fixed-point precision export, also used by the A/B fixture.
if(NOT TARGET cineol_clean_core)
    set(_precision_root "${CMAKE_SOURCE_DIR}/native-hall")
    set(_precision_include "${CMAKE_BINARY_DIR}/native-hall/precision/generated")
    file(GLOB _precision_sources CONFIGURE_DEPENDS
        "${_precision_root}/core/*.hpp" "${_precision_root}/core/*.cpp" "${_precision_root}/core/*.inc"
        "${_precision_root}/desktop/*.hpp" "${_precision_root}/desktop/*.inc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_precision_sources}
        "${_precision_root}/clean-ab/generate_precision.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${_precision_root}/clean-ab/generate_precision.py"
        "${_precision_root}" "${_precision_include}/clean" RESULT_VARIABLE _precision_result)
    if(NOT _precision_result EQUAL 0)
        message(FATAL_ERROR "Fixed-point precision export failed")
    endif()
    add_library(cineol_clean_core STATIC "${_precision_include}/clean/core/hall.cpp")
    target_include_directories(cineol_clean_core PUBLIC "${_precision_include}")
    set_target_properties(cineol_clean_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET cineol_clean_core PROPERTY CINEOL_PRECISION_INCLUDE "${_precision_include}")
endif()
