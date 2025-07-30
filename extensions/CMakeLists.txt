cmake_minimum_required(VERSION 3.20)
project(MULOExtensions)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type to Release to avoid Debug crashes
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Export compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Output directories per build type and platform (mirroring main CMakeLists.txt)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/RelWithDebInfo)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL    ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/MinSizeRel)

# Platform-specific JUCE (using relative paths for self-contained toolkit)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Build full JUCE for extensions 
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

# Linux-only GTK/WebKit (if needed) - same as main CMakeLists.txt
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.0)
endif()

# Engine source files for extensions (using local structure)
set(ENGINE_SOURCES
    "src/audio/Engine.cpp"
)

# Common include directories for all extensions (self-contained)
set(COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/includes
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/UILO
    ${SFML_INCLUDE_DIRS}
    
    # Platform-specific includes
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${GTK3_INCLUDE_DIRS}>
    $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${WEBKIT2GTK_INCLUDE_DIRS}>
    
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

# Function to automatically create extension wrapper and library
function(add_extension EXTENSION_NAME)
    # Create wrapper .cpp file automatically
    set(WRAPPER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${EXTENSION_NAME}.cpp")
    file(WRITE ${WRAPPER_FILE} "#include \"${CMAKE_SOURCE_DIR}/extensions_src/${EXTENSION_NAME}.hpp\"\n")
    
    # Create shared library
    add_library(${EXTENSION_NAME} SHARED 
        ${WRAPPER_FILE}
        ${ENGINE_SOURCES}
    )
    
    target_include_directories(${EXTENSION_NAME} PRIVATE ${COMMON_INCLUDE_DIRS})
    target_compile_definitions(${EXTENSION_NAME} PRIVATE ${COMMON_COMPILE_DEFINITIONS})
endfunction()

# Automatically discover and build all extensions from extensions_src
file(GLOB EXTENSION_HEADERS "${CMAKE_SOURCE_DIR}/extensions_src/*.hpp")
set(EXTENSION_TARGETS)

foreach(HEADER_FILE ${EXTENSION_HEADERS})
    get_filename_component(EXTENSION_NAME ${HEADER_FILE} NAME_WE)
    add_extension(${EXTENSION_NAME})
    list(APPEND EXTENSION_TARGETS ${EXTENSION_NAME})
endforeach()

# Common compile definitions
set(COMMON_COMPILE_DEFINITIONS
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# Link libraries for all extensions
foreach(target ${EXTENSION_TARGETS})
    target_link_libraries(${target} PRIVATE
        # JUCE libraries
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_dsp
        
        # SFML
        SFML::Graphics
        SFML::Window
        SFML::System
        
        # JSON library
        nlohmann_json::nlohmann_json
        
        # Platform-specific libraries
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:curl>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${GTK3_LIBRARIES}>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${WEBKIT2GTK_LIBRARIES}>
    )
endforeach()
