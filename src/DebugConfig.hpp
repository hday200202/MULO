#pragma once

// Debug configuration for MULO
// Uncomment the line below to enable debug output
#define MULO_DEBUG

#ifdef MULO_DEBUG
    #include <iostream>
    #include <iomanip>
    #include <fstream>
    
    inline std::ofstream& getDebugLog() {
        static std::ofstream dbLog("log.txt");
        return dbLog;
    }
    
    #define DEBUG_PRINT(x) getDebugLog() << x << std::endl
    #define DEBUG_PRINT_INLINE(x) getDebugLog() << x; getDebugLog().flush()
    #define DEBUG_PRINT_FORMATTED(x, y) getDebugLog() << x << y << std::endl
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINT_INLINE(x)
    #define DEBUG_PRINT_FORMATTED(x, y)
#endif
