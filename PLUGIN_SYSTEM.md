# MULO Plugin System

## Overview

The MULO plugin system allows you to create dynamic components that can be loaded at runtime. Plugins are compiled as shared libraries (.so/.dll/.dylib) and loaded dynamically by the main application.

## Plugin Architecture

### Core Components

1. **PluginVTable**: C-style interface for cross-library compatibility
2. **PluginComponentWrapper**: C++ wrapper that bridges plugins with MULOComponent
3. **DECLARE_PLUGIN**: Macro for simplified plugin creation
4. **Plugin Loading**: Dynamic library loading system in Application class

### Plugin Interface

All plugins must implement the following interface:

```cpp
typedef struct {
    void* instance;                    // Plugin instance pointer
    void (*init)(void* instance, Application* app, Engine* engine, UIResources* resources, UIState* uiState);
    void (*update)(void* instance);    // Called every frame
    bool (*handleEvents)(void* instance);  // Handle UI events, return true if UI needs update
    bool (*isInitialized)(void* instance);
    void (*destroy)(void* instance);   // Clean up plugin
    const char* (*getName)(void* instance);
    void (*show)(void* instance);      // Show plugin UI
    void (*hide)(void* instance);      // Hide plugin UI
    bool (*isVisible)(void* instance);
    void (*setVisible)(void* instance, bool visible);
    void (*toggle)(void* instance);
    void* (*getLayout)(void* instance); // Returns Container* for UI layout
} PluginVTable;
```

## Plugin Types

### 1. MULOComponent-based Plugins (ExamplePlugin.hpp)

These inherit from MULOComponent and use the DECLARE_PLUGIN macro:

```cpp
class ExamplePlugin : public MULOComponent {
public:
    ExamplePlugin() { name = "example_plugin"; }
    
    void init() override {
        // Initialize plugin UI and logic
    }
    
    void update() override {
        // Update plugin state
    }
    
    bool handleEvents() override {
        // Handle UI events
        return false;
    }
};

// Generate plugin interface
DECLARE_PLUGIN(ExamplePlugin)
```

**Requirements**: Full JUCE setup, all MULO dependencies
**Benefits**: Full access to MULOComponent features, automatic interface generation

### 2. Standalone Plugins (SimplePlugin.hpp)

Self-contained plugins with minimal dependencies:

```cpp
class SimpleExamplePlugin {
    // Plugin implementation
};

extern "C" {
    // Manual C interface implementation
    PluginVTable* createPlugin() {
        // Return plugin vtable
    }
}
```

**Requirements**: Only UILO library
**Benefits**: Minimal dependencies, faster compilation, easier distribution

## Building Plugins

### Method 1: Build Script (Recommended)

```bash
./build_plugins.sh
```

This builds all plugins and places them in `bin/plugins/`.

### Method 2: Manual Compilation

For standalone plugins:
```bash
g++ -I./external -std=c++20 -fPIC -shared -o plugin.so plugin.hpp
```

For MULOComponent plugins (requires JUCE):
```bash
g++ -I./src -I./external -I./external/linux/JUCE/modules -std=c++20 -fPIC -shared -o plugin.so plugin.hpp
```

## Plugin Loading

The Application class automatically scans and loads plugins:

```cpp
// In Application.cpp
void scanAndLoadPlugins(const std::string& pluginDir) {
    // Scan directory for .so/.dll/.dylib files
    // Load each plugin using dlopen/LoadLibrary
    // Call createPlugin() function
    // Wrap in PluginComponentWrapper
}
```

### Plugin Discovery

Plugins are loaded from:
- `bin/plugins/` directory
- Platform-specific extensions: `.so` (Linux), `.dll` (Windows), `.dylib` (macOS)

## Plugin Development Guide

### 1. Choose Plugin Type

- **MULOComponent-based**: For complex UI plugins that need full framework access
- **Standalone**: For simple plugins or when minimizing dependencies

### 2. Implement Required Methods

All plugins must implement:
- `init()`: Initialize plugin with host references
- `update()`: Called every frame
- `handleEvents()`: Process UI events
- `getLayout()`: Return UI container

### 3. Build and Test

1. Write plugin header file
2. Run `./build_plugins.sh`
3. Test loading in main application

### 4. Distribution

Distribute the compiled `.so/.dll/.dylib` file. Users place it in the `bin/plugins/` directory.

## Advanced Features

### Host Access

Plugins receive references to:
- **Application**: Main application instance
- **Engine**: Audio engine for playback control
- **UIResources**: Fonts, themes, resources
- **UIState**: Current UI state

### UI Integration

Plugins can:
- Create custom UI layouts using UILO
- Access host themes and fonts
- Integrate with main application UI

### Error Handling

- Check for null references in `init()`
- Handle missing dependencies gracefully
- Use proper RAII for resource management

## Troubleshooting

### Common Issues

1. **JUCE Dependencies**: MULOComponent plugins require full JUCE setup
2. **Include Paths**: Ensure correct paths to UILO and other dependencies
3. **Symbol Export**: Use `extern "C"` for plugin interface functions
4. **Memory Management**: Plugins are responsible for cleanup

### Debug Tips

1. Check plugin compilation separately
2. Verify `createPlugin()` function export
3. Test plugin loading with minimal host application
4. Use debug prints in plugin functions

## Example Plugins

- **SimplePlugin.hpp**: Minimal standalone plugin with UILO UI
- **ExamplePlugin.hpp**: Full MULOComponent plugin with comprehensive features

## Future Enhancements

- Plugin versioning system
- Hot-reloading capabilities
- Plugin dependency management
- Standardized plugin metadata
- Visual plugin manager in main application
