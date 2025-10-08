#pragma once

// Debug configuration for MULO
// Uncomment the line below to enable debug output
// #define MULO_DEBUG

#ifdef MULO_DEBUG
    #include <iostream>
    #include <iomanip>
    #define DEBUG_PRINT(x) std::cout << x << std::endl
    #define DEBUG_PRINT_INLINE(x) std::cout << x
    #define DEBUG_PRINT_FORMATTED(x, y) std::cout << x << y << std::endl
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINT_INLINE(x)
    #define DEBUG_PRINT_FORMATTED(x, y)
#endif
