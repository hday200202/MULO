#pragma once

// Debug configuration for MDAW
// Uncomment the line below to enable debug output
// #define MDAW_DEBUG

#ifdef MDAW_DEBUG
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
