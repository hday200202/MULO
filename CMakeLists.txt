cmake_minimum_required(VERSION 3.20)
project(MULO)

# Export compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories per build type and platform
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL    ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/MinSizeRel)

# Platform-specific JUCE
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(external/linux/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/linux/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_subdirectory(external/mac/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/mac/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(external/windows/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/windows/JUCE/modules")
endif()

# Enable JUCE modal loops so FileChooser::browseForFileToOpen() is available
add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED=1)

# SFML Setup
set(SFML_INCLUDE_DIRS "")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SFML_PC REQUIRED IMPORTED_TARGET
        sfml-graphics
        sfml-window
        sfml-system
    )
    add_library(SFML::Graphics ALIAS PkgConfig::SFML_PC)
    add_library(SFML::Window   ALIAS PkgConfig::SFML_PC)
    add_library(SFML::System   ALIAS PkgConfig::SFML_PC)
    set(SFML_INCLUDE_DIRS ${SFML_PC_INCLUDE_DIRS})
    find_package(CURL REQUIRED)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SFML_DIR "${CMAKE_SOURCE_DIR}/external/windows/SFML-3.0.0-MSVC/lib/cmake/SFML")
    find_package(SFML 3.0.0 COMPONENTS Graphics Window System REQUIRED)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
endif()

# JSON for config handling
find_package(nlohmann_json CONFIG REQUIRED)

# Gather source files
file(GLOB_RECURSE MULO_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/frontend/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/audio/*.cpp"
)

# Define executable
add_executable(MULO ${MULO_SOURCES})

# Add JUCE module headers to includes
target_include_directories(MULO PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/frontend"
    "${CMAKE_SOURCE_DIR}/src/audio"
    "${CMAKE_SOURCE_DIR}/external"
    ${SFML_INCLUDE_DIRS}

    # JUCE modules
    ${JUCE_MODULE_DIR}
    ${JUCE_MODULE_DIR}/juce_core/include
    ${JUCE_MODULE_DIR}/juce_events/include
    ${JUCE_MODULE_DIR}/juce_data_structures/include
    ${JUCE_MODULE_DIR}/juce_graphics/include
    ${JUCE_MODULE_DIR}/juce_gui_basics/include
    ${JUCE_MODULE_DIR}/juce_gui_extra/include
    ${JUCE_MODULE_DIR}/juce_audio_basics/include
    ${JUCE_MODULE_DIR}/juce_audio_devices/include
    ${JUCE_MODULE_DIR}/juce_audio_formats/include
    ${JUCE_MODULE_DIR}/juce_audio_utils/include
    ${JUCE_MODULE_DIR}/juce_dsp/include
    ${JUCE_MODULE_DIR}/juce_gui_extra/gui_extra
)

# Linux-only GTK/WebKit (if needed)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.0)
    target_include_directories(MULO PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT2GTK_INCLUDE_DIRS})
    target_link_libraries(MULO PRIVATE ${GTK3_LIBRARIES} ${WEBKIT2GTK_LIBRARIES})
    add_definitions(${GTK3_CFLAGS_OTHER} ${WEBKIT2GTK_CFLAGS_OTHER})
endif()

# Link libraries
target_link_libraries(MULO PRIVATE
    # JUCE Core GUI
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics

    # JUCE GUI Extras (FileChooser etc.)
    juce::juce_gui_extra

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

    # JSON library for config file I/O
    nlohmann_json::nlohmann_json

    # Curl (Linux only)
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:curl>
)

# Pass modal-loops define to MULO too
target_compile_definitions(MULO PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# Compiler definitions
target_compile_definitions(MULO PRIVATE
    JUCE_SHARED_CODE=1
    JUCE_STANDALONE_APPLICATION=1
)

# Install target
install(TARGETS MULO DESTINATION bin)

# Windows: Copy SFML DLLs after build
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

# Copy assets to output directory
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:MULO>/assets"
    COMMENT "Copying assets folder to $<TARGET_FILE_DIR:MULO>/assets"
)

