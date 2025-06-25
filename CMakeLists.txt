cmake_minimum_required(VERSION 3.20)

project(MULO)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(external/linux/JUCE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_subdirectory(external/mac/JUCE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(external/windows/JUCE)
endif()

set(_SFML_INCLUDE_DIRS "")

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
    set(_SFML_INCLUDE_DIRS ${SFML_PC_INCLUDE_DIRS})
else()
    find_package(SFML CONFIG REQUIRED COMPONENTS graphics window system)
endif()

find_package(CURL REQUIRED)

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
    ${_SFML_INCLUDE_DIRS}
)

target_link_libraries(MULO PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    SFML::Graphics
    SFML::Window
    SFML::System
    curl
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(MULO PRIVATE JUCE_LINUX=1)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin") # macOS
    target_compile_definitions(MULO PRIVATE JUCE_MAC=1)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(MULO PRIVATE JUCE_WINDOWS=1)
endif()

target_compile_definitions(MULO PRIVATE
    JUCE_SHARED_CODE=1
    JUCE_STANDALONE_APPLICATION=1
)

install(TARGETS MULO DESTINATION bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/${CMAKE_BUILD_TYPE})