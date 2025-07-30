# MULO Plugin Development Guide

## Overview
This guide explains how to convert MULOComponents into plugins and how to build/manage the plugin system in MDAW.

## Plugin System Architecture

### Core Components
- **Plugin Interface**: C-style `PluginVTable` with `getPluginInterface()` function
- **Plugin Macro**: `DECLARE_PLUGIN(ComponentName)` for automatic interface implementation
- **Plugin Loading**: Dynamic library loading with symbol resolution
- **Build System**: CMake functions for building plugins as shared libraries

## Converting a Component to a Plugin

### Step 1: Add Plugin Interface
At the end of your component's header file (`.hpp`), add:

```cpp
// Plugin interface implementation
DECLARE_PLUGIN(YourComponentName)
```

**Example:**
```cpp
class MixerComponent : public MULOComponent {
    // ... your component implementation
};

// Plugin interface implementation  
DECLARE_PLUGIN(MixerComponent)
```

### Step 2: Place in Examples Directory
Move your component header file to the `/examples/` directory:
```bash
cp src/frontend/YourComponent.hpp examples/
```

### Step 3: Add to CMakeLists.txt
In `/examples/CMakeLists.txt`, add your plugin to the build list:
```cmake
# Add your component as a plugin
add_mulo_plugin(YourComponentName)
```

### Step 4: Build the Plugin
Use the build script:
```bash
cd examples
./build.sh
```

## Build System Details

### Plugin Build Function
The `add_mulo_plugin()` function automatically:
- Creates a source file that includes your header
- Links with Engine sources for full functionality
- Sets up JUCE include paths (headers only, no linking)
- Configures SFML and other dependencies
- Outputs to `examples/build/plugins/`
- Copies to main `plugins/` directory

### Dependencies Included
Each plugin gets access to:
- Full Engine API (audio processing, tracks, clips)
- SFML graphics and windowing
- UILO UI framework
- JSON configuration handling
- All JUCE headers (without linking to avoid conflicts)

## Plugin Loading in Application

### Automatic Discovery
The main application automatically:
- Scans `plugins/` directory for `.so` files
- Loads each plugin and resolves `getPluginInterface`
- Creates component instances through the plugin interface
- Manages plugin lifecycle (load/unload)

### Plugin Registration
Plugins are accessible by name in the application:
```cpp
// Plugin is automatically registered with lowercase name
app->loadPlugin("mixer");    // loads MixerComponent plugin
app->loadPlugin("timeline"); // loads TimelineComponent plugin
```

## Working Examples

### Successfully Converted Plugins
1. **TimelineComponent** - Complete timeline with tracks, clips, waveforms
2. **SettingsComponent** - Settings dialog with theme and audio config
3. **MixerComponent** - Full mixer with volume/pan controls for all tracks

### Plugin Files Structure
```
examples/
├── CMakeLists.txt           # Plugin build configuration
├── build.sh                 # Automated build script
├── TimelineComponent.hpp    # Timeline plugin
├── SettingsComponent.hpp    # Settings plugin
├── MixerComponent.hpp       # Mixer plugin
└── build/
    └── plugins/
        ├── libTimelineComponent.so
        ├── libSettingsComponent.so
        └── libMixerComponent.so
```

## Common Issues and Solutions

### Missing getPluginInterface Function
**Error:** `undefined symbol: getPluginInterface`

**Solution:** Add `DECLARE_PLUGIN(ComponentName)` at the end of your header file.

### JUCE Symbol Conflicts
**Error:** Double-destruction or undefined JUCE symbols

**Solution:** The build system includes JUCE headers without linking. This is already configured in `add_mulo_plugin()`.

### Build Failures
**Error:** Cannot find headers or linking issues

**Solution:** 
1. Ensure your component includes `Application.hpp`
2. Use the provided build script: `./build.sh`
3. Check that JUCE paths are correct in CMakeLists.txt

## Development Workflow

### For New Plugins
1. Create your component inheriting from `MULOComponent`
2. Implement required virtual methods (`init`, `update`, `handleEvents`)
3. Add `DECLARE_PLUGIN(ComponentName)` at the end
4. Place in `examples/` directory
5. Add to `examples/CMakeLists.txt`
6. Build with `./build.sh`

### For Testing
1. Build main application: `./build.sh` (from root)
2. Build plugins: `cd examples && ./build.sh`
3. Run main application - plugins auto-load from `plugins/` directory
4. Check console output for plugin loading status

### For Distribution
Plugins are self-contained shared libraries that can be:
- Distributed separately from main application
- Hot-swapped while application is running (with proper unload/reload)
- Developed independently by different team members

## Architecture Benefits

### Modularity
- Components can be developed and tested independently
- Reduces main application binary size
- Enables selective feature loading

### Extensibility
- Third-party developers can create plugins
- Easy to add new UI components without recompiling main app
- Plugin system supports runtime loading/unloading

### Development Efficiency
- Faster plugin compilation vs full application rebuild
- Parallel development of different components
- Isolated testing of individual features

## Technical Notes

### Plugin Interface
```cpp
struct PluginVTable {
    MULOComponent* (*createComponent)();
    void (*destroyComponent)(MULOComponent*);
    const char* (*getName)();
    const char* (*getVersion)();
};
```

### Symbol Visibility
Plugins use `__attribute__((visibility("default")))` to export the `getPluginInterface` symbol while keeping other symbols private.

### Memory Management
- Plugin components are created/destroyed through the plugin interface
- Engine access is provided through included source files
- RAII principles ensure proper cleanup on plugin unload

## Future Enhancements

### Planned Features
- Plugin versioning and compatibility checking
- Plugin configuration and settings persistence
- Hot-reload capability for development
- Plugin dependency management
- Plugin marketplace/registry system

This plugin system provides a solid foundation for modular MULO development while maintaining full access to the Engine API and UI frameworks.
