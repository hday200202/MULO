cmake_minimum_required(VERSION 3.20)

project(MULO)

# Set output directories for all build types and platforms
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/MinSizeRel)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(external/linux/JUCE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_subdirectory(external/mac/JUCE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(external/windows/JUCE)
endif()

set(SFML_INCLUDE_DIRS "")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SFML_PC REQUIRED IMPORTED_TARGET
        sfml-graphics
        sfml-window
        sfml-system
    )
    add_library(SFML::Graphics ALIAS PkgConfig::SFML_PC)
    add_library(SFML::Window ALIAS PkgConfig::SFML_PC)
    add_library(SFML::System ALIAS PkgConfig::SFML_PC)
    set(SFML_INCLUDE_DIRS ${SFML_PC_INCLUDE_DIRS})

elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SFML_DIR "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/lib/cmake/SFML")
    find_package(SFML COMPONENTS Graphics Window System REQUIRED)

endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(CURL REQUIRED)
endif()

file(GLOB_RECURSE MULO_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/frontend/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/audio/*.cpp"
)

add_executable(MULO ${MULO_SOURCES})

target_include_directories(MULO PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/frontend"
    "${CMAKE_SOURCE_DIR}/src/audio"
    "${CMAKE_SOURCE_DIR}/external"
    ${SFML_INCLUDE_DIRS}
)

target_link_libraries(MULO PRIVATE
    # JUCE Core GUI Stuff
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics

    # JUCE Audio
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_dsp

    # SFML
    SFML::Graphics
    SFML::Window
    SFML::System

    # Curl (Linux only)
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:curl>
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(MULO PRIVATE JUCE_LINUX=1)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(MULO PRIVATE JUCE_MAC=1)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(MULO PRIVATE JUCE_WINDOWS=1)
endif()

target_compile_definitions(MULO PRIVATE
    JUCE_SHARED_CODE=1
    JUCE_STANDALONE_APPLICATION=1
)

install(TARGETS MULO DESTINATION bin)

# Copy SFML DLLs on Windows after build
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SFML_DLLS_RELEASE
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-graphics-3.dll"
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-window-3.dll"
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-system-3.dll"
    )
    set(SFML_DLLS_DEBUG
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-graphics-d-3.dll"
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-window-d-3.dll"
        "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/bin/sfml-system-d-3.dll"
    )

    set(RELEASE_DIR "${CMAKE_SOURCE_DIR}/bin/Windows/Release")
    set(DEBUG_DIR   "${CMAKE_SOURCE_DIR}/bin/Windows/Debug")

    add_custom_target(copy_sfml_dlls ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEBUG_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SFML_DLLS_RELEASE} "${RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SFML_DLLS_DEBUG}   "${DEBUG_DIR}"
        COMMENT "Copying SFML DLLs to output folders"
    )
endif()

# Copy assets folder to output directory for all platforms after build
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MULO>/assets"
    COMMENT "Copying assets folder to $<TARGET_FILE_DIR:MULO>/assets"
)