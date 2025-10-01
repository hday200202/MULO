cmake_minimum_required(VERSION 3.20)
project(MULO)

# Ensure 64-bit build on Windows
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "32-bit builds are not supported on Windows. Please use a 64-bit generator:")
        message(FATAL_ERROR "  cmake -G \"Visual Studio 17 2022\" -A x64 ..")
        message(FATAL_ERROR "  or")
        message(FATAL_ERROR "  cmake -G \"Visual Studio 16 2019\" -A x64 ..")
    endif()
    message(STATUS "Building for 64-bit Windows (pointer size: ${CMAKE_SIZEOF_VOID_P} bytes)")
endif()

# Build optimizations for faster compilation
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use ccache if available for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache for faster builds")
endif()

# Export compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories per build type and platform
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG      ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_SOURCE_DIR}/bin/${CMAKE_SYSTEM_NAME}/MinSizeRel)

# Platform-specific JUCE - Optimized build
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # JUCE build optimizations
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Disable JUCE extras")
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Disable JUCE examples") 
    set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "Disable source groups")
    
    add_subdirectory(external/linux/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/linux/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Disable JUCE extras")
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Disable JUCE examples")
    set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "Disable source groups")
    
    add_subdirectory(external/mac/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/mac/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Disable JUCE extras")
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Disable JUCE examples")
    set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF CACHE BOOL "Disable source groups")
    
    add_subdirectory(external/windows/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/external/windows/JUCE/modules")
endif()

# Enable JUCE modal loops so FileChooser::browseForFileToOpen() is available
add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED=1)

# Enable VST3 plugin support globally for JUCE before JUCE is added
set(JUCE_PLUGINHOST_VST3 ON CACHE BOOL "Enable VST3 plugin support" FORCE)
add_compile_definitions(JUCE_PLUGINHOST_VST3=1)

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
    
    # Check if SFML path exists before trying to find it
    if(NOT EXISTS "${SFML_DIR}/SFMLConfig.cmake")
        message(FATAL_ERROR "SFML not found at: ${SFML_DIR}/SFMLConfig.cmake. Please ensure SFML 3.0.0 (64-bit MSVC) is installed in external/windows/SFML-3.0.0-MSVC/")
    endif()
    
    find_package(SFML 3.0.0 COMPONENTS Graphics Window System REQUIRED)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
endif()

# JSON for config handling
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via package manager, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

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
    ${JUCE_MODULE_DIR}/juce_audio_processors/include
    ${JUCE_MODULE_DIR}/juce_audio_utils/include
    ${JUCE_MODULE_DIR}/juce_dsp/include
    ${JUCE_MODULE_DIR}/juce_gui_extra/gui_extra
    ${JUCE_MODULE_DIR}/juce_opengl/include
)

# Linux-only GTK/WebKit (if needed)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.1)
    target_include_directories(MULO PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT2GTK_INCLUDE_DIRS})
    target_link_libraries(MULO PRIVATE ${GTK3_LIBRARIES} ${WEBKIT2GTK_LIBRARIES})
    add_definitions(${GTK3_CFLAGS_OTHER} ${WEBKIT2GTK_CFLAGS_OTHER})
endif()

# Firebase configuration for marketplace functionality
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/external/linux/firebase/include")
    set(FIREBASE_LIBS
        "${CMAKE_SOURCE_DIR}/external/linux/firebase/libfirebase_app.a"
        "${CMAKE_SOURCE_DIR}/external/linux/firebase/libfirebase_firestore.a"
        "${CMAKE_SOURCE_DIR}/external/linux/firebase/libfirebase_auth.a"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/external/mac/firebase/include")
    set(FIREBASE_LIBS
        "${CMAKE_SOURCE_DIR}/external/mac/firebase/libfirebase_app.a"
        "${CMAKE_SOURCE_DIR}/external/mac/firebase/libfirebase_firestore.a"
        "${CMAKE_SOURCE_DIR}/external/mac/firebase/libfirebase_auth.a"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/external/windows/firebase/include")
    set(FIREBASE_LIBS
        "${CMAKE_SOURCE_DIR}/external/windows/firebase/firebase_app.lib"
        "${CMAKE_SOURCE_DIR}/external/windows/firebase/firebase_firestore.lib"
        "${CMAKE_SOURCE_DIR}/external/windows/firebase/firebase_auth.lib"
    )
endif()

# Check if Firebase libraries exist and add them
set(FIREBASE_LIBS_FOUND "")
foreach(lib ${FIREBASE_LIBS})
    if (EXISTS "${lib}")
        list(APPEND FIREBASE_LIBS_FOUND "${lib}")
    endif()
endforeach()

if (FIREBASE_LIBS_FOUND AND EXISTS "${FIREBASE_INCLUDE}")
    message(STATUS "Firebase found - enabling Firebase support")
    target_include_directories(MULO PRIVATE "${FIREBASE_INCLUDE}")
    target_link_libraries(MULO PRIVATE ${FIREBASE_LIBS_FOUND})
    target_compile_definitions(MULO PRIVATE FIREBASE_AVAILABLE=1)
    
    # Add Firebase system dependencies on Linux
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SECRET REQUIRED libsecret-1)
        target_link_libraries(MULO PRIVATE ${SECRET_LIBRARIES})
    endif()
else()
    message(STATUS "Firebase not found - using mock data")
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
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp

    # JUCE OpenGL
    juce::juce_opengl

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
    
    # Enable VST plugin support
    JUCE_PLUGINHOST_VST3=1
)

# Enable symbol export for plugins on Linux
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(MULO PROPERTIES
        ENABLE_EXPORTS ON
        LINK_FLAGS "-Wl,--export-dynamic"
    )
endif()

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

# Add tinyfiledialogs library
add_library(tinyfiledialogs STATIC external/tinyfiledialogs/tinyfiledialogs.cpp)
target_include_directories(tinyfiledialogs PUBLIC external/tinyfiledialogs)
target_link_libraries(MULO PRIVATE tinyfiledialogs)

if(UNIX AND NOT APPLE)
    target_link_libraries(MULO PRIVATE X11)
endif()