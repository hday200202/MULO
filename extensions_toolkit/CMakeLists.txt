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


# Output directories per build type and platform (output to local bin folder)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(EXT_PLATFORM Linux)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(EXT_PLATFORM Mac)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(EXT_PLATFORM Windows)
else()
    set(EXT_PLATFORM ${CMAKE_SYSTEM_NAME})
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG         ${CMAKE_SOURCE_DIR}/bin/${EXT_PLATFORM}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE       ${CMAKE_SOURCE_DIR}/bin/${EXT_PLATFORM}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin/${EXT_PLATFORM}/RelWithDebInfo)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL    ${CMAKE_SOURCE_DIR}/bin/${EXT_PLATFORM}/MinSizeRel)

# Platform-specific JUCE (using relative paths for self-contained toolkit)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../external/linux/JUCE ${CMAKE_BINARY_DIR}/JUCE)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/../external/linux/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../external/mac/JUCE ${CMAKE_BINARY_DIR}/JUCE_mac)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/../external/mac/JUCE/modules")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../external/windows/JUCE ${CMAKE_BINARY_DIR}/JUCE_win)
    set(JUCE_MODULE_DIR "${CMAKE_SOURCE_DIR}/../external/windows/JUCE/modules")
endif()

# Enable JUCE modal loops so FileChooser::browseForFileToOpen() is available
add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED=1)

# Enable VST3 plugin support for extensions
set(JUCE_PLUGINHOST_VST3 ON CACHE BOOL "Enable VST3 plugin support" FORCE)
add_compile_definitions(JUCE_PLUGINHOST_VST3=1)

# SFML Setup
set(SFML_INCLUDE_DIRS "")
set(SFML_FOUND FALSE)

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
    set(SFML_FOUND TRUE)
    find_package(CURL REQUIRED)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(SFML_DIR "${CMAKE_SOURCE_DIR}/../external/windows/SFML-3.0.0-MSVC/lib/cmake/SFML")
    
    # Check if SFML installation exists and has required files
    if(EXISTS "${CMAKE_SOURCE_DIR}/../external/windows/SFML-3.0.0-MSVC/lib/cmake/SFML/SFMLConfig.cmake")
        find_package(SFML 3.0.0 COMPONENTS Graphics Window System QUIET)
        if(SFML_FOUND)
            set(SFML_FOUND TRUE)
        else()
            message(WARNING "SFML found but configuration failed. This might be due to missing library files.")
            message(WARNING "Please ensure SFML is properly installed in: ${CMAKE_SOURCE_DIR}/../external/windows/SFML-3.0.0-MSVC")
            message(WARNING "Continuing build without SFML...")
        endif()
    else()
        message(WARNING "SFML not found at: ${CMAKE_SOURCE_DIR}/../external/windows/SFML-3.0.0-MSVC")
        message(WARNING "Please install SFML 3.0.0 for Windows (MSVC) to enable SFML features.")
        message(WARNING "Continuing build without SFML...")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
    set(SFML_FOUND TRUE)
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

# Linux-only GTK/WebKit (if needed) - same as main CMakeLists.txt
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2GTK REQUIRED webkit2gtk-4.0)
endif()

# Engine source files for extensions (using local structure)
set(ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/../src/audio/Engine.cpp"
    "${CMAKE_SOURCE_DIR}/../src/audio/AudioClip.cpp"
    "${CMAKE_SOURCE_DIR}/../src/audio/Composition.cpp"
    "${CMAKE_SOURCE_DIR}/../src/audio/Track.cpp"
    "${CMAKE_SOURCE_DIR}/../src/audio/Effect.cpp"
    "${CMAKE_SOURCE_DIR}/../src/frontend/Application.cpp"
    "${CMAKE_SOURCE_DIR}/../src/frontend/FileTree.cpp"
)

set(COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/../src
    ${CMAKE_SOURCE_DIR}/../src/audio
    ${CMAKE_SOURCE_DIR}/../src/frontend
    ${CMAKE_SOURCE_DIR}/../external
    ${CMAKE_SOURCE_DIR}/../external/UILO
    ${CMAKE_SOURCE_DIR}/../external/tinyfiledialogs
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
    JUCE_SHARED_CODE=1
)

# Add tinyfiledialogs library for extensions
add_library(tinyfiledialogs STATIC ${CMAKE_SOURCE_DIR}/../external/tinyfiledialogs/tinyfiledialogs.cpp)
target_include_directories(tinyfiledialogs PUBLIC ${CMAKE_SOURCE_DIR}/../external/tinyfiledialogs)
set_target_properties(tinyfiledialogs PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link libraries for all extensions

# Link libraries for all extensions, and link Firebase only to MarketplaceComponent if it exists
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
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        
        # JSON library
        nlohmann_json::nlohmann_json
        
        # tinyfiledialogs
        tinyfiledialogs
        
        # Platform-specific libraries
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:curl>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${GTK3_LIBRARIES}>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${WEBKIT2GTK_LIBRARIES}>
        $<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:X11>
    )

    # Link Firebase and add include dir only to MarketplaceComponent
    if (target STREQUAL "MarketplaceComponent")
        # Detect platform and set Firebase lib/include paths
        if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(FIREBASE_LIB "${CMAKE_SOURCE_DIR}/../external/linux/firebase/libfirebase.a")
            set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/../external/linux/firebase/include")
        elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(FIREBASE_LIB "${CMAKE_SOURCE_DIR}/../external/mac/firebase/libfirebase.a")
            set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/../external/mac/firebase/include")
        elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(FIREBASE_INCLUDE "${CMAKE_SOURCE_DIR}/../external/windows/firebase/include")
            # List all required Firebase .lib files for Windows (adjust as needed for your SDK)
            set(FIREBASE_LIBS
                "${CMAKE_SOURCE_DIR}/../external/windows/firebase/firebase_app.lib"
                "${CMAKE_SOURCE_DIR}/../external/windows/firebase/firebase_firestore.lib"
                "${CMAKE_SOURCE_DIR}/../external/windows/firebase/firebase_auth.lib"
                "${CMAKE_SOURCE_DIR}/../external/windows/firebase/firebase.lib"
                "${CMAKE_SOURCE_DIR}/../external/windows/firebase/firebase_storage.lib"
                # Add more .lib files if your SDK provides them and you use those features
            )
            # List required Windows system libraries for Firebase
            set(FIREBASE_SYSTEM_LIBS
                bcrypt
                crypt32
                ncrypt
                ws2_32
            )
        endif()
        # Link all found Firebase libs
        set(FIREBASE_LIBS_FOUND "")
        foreach(lib ${FIREBASE_LIBS})
            if (EXISTS "${lib}")
                list(APPEND FIREBASE_LIBS_FOUND "${lib}")
            else()
                message(WARNING "Firebase library not found for MarketplaceComponent: ${lib}")
            endif()
        endforeach()
        if (FIREBASE_LIBS_FOUND)
            target_link_libraries(${target} PRIVATE ${FIREBASE_LIBS_FOUND})
            message(STATUS "Linked Firebase libs to MarketplaceComponent: ${FIREBASE_LIBS_FOUND}")
        else()
            message(WARNING "No Firebase libraries found for MarketplaceComponent!")
        endif()
        # Link Windows system libraries if on Windows
        if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            target_link_libraries(${target} PRIVATE ${FIREBASE_SYSTEM_LIBS})
            message(STATUS "Linked Windows system libs to MarketplaceComponent: ${FIREBASE_SYSTEM_LIBS}")
        endif()
        if (EXISTS "${FIREBASE_INCLUDE}")
            target_include_directories(${target} PRIVATE "${FIREBASE_INCLUDE}")
            message(STATUS "Added Firebase include dir to MarketplaceComponent: ${FIREBASE_INCLUDE}")
        else()
            message(WARNING "Firebase include dir not found for MarketplaceComponent: ${FIREBASE_INCLUDE}")
        endif()
    endif()

    # Apply common compile definitions
    target_compile_definitions(${target} PRIVATE ${COMMON_COMPILE_DEFINITIONS})

    # Link SFML if found
    if(SFML_FOUND)
        target_link_libraries(${target} PRIVATE
            SFML::Graphics
            SFML::Window
            SFML::System
        )
        target_compile_definitions(${target} PRIVATE SFML_AVAILABLE=1)
    else()
        message(STATUS "Building ${target} without SFML support")
    endif()

    # Platform-specific settings
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Enable symbol export for plugins on Linux
        set_target_properties(${target} PROPERTIES
            ENABLE_EXPORTS ON
            LINK_FLAGS "-Wl,--export-dynamic"
        )
        target_compile_options(${target} PRIVATE 
            ${GTK3_CFLAGS_OTHER} 
            ${WEBKIT2GTK_CFLAGS_OTHER}
        )
    endif()
endforeach()

# Remove 'lib' prefix from all extension shared libraries
foreach(target ${EXTENSION_TARGETS})
    set_target_properties(${target} PROPERTIES PREFIX "")
endforeach()
