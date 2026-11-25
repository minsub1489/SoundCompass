#pragma once

#include <cmath>
#include <cstdio>

inline int g_failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

inline bool Near(double a, double b, double tolerance = 1e-4) { return std::fabs(a - b) <= tolerance; }

#define RUN(test)                                                                      \
    do {                                                                               \
        const int before = g_failures;                                                 \
        test();                                                                        \
        std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", #test);        \
    } while (0)
