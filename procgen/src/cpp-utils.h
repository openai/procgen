#pragma once

#include <set>
#include <cctype>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

// MSVC requires _USE_MATH_DEFINES to be set to define M_PI
// and M_PI is interpreted as a double instead of a float anyway
const float PI = 3.14159265358979323846264338327950288f;

// assert() is disabled in release mode, use a custom assert function instead
#define fassert(cond)                                                            \
    do {                                                                         \
        if (!(cond)) {                                                           \
            printf("fassert failed '%s' at %s:%d\n", #cond, __FILE__, __LINE__); \
            exit(EXIT_FAILURE);                                                  \
        }                                                                        \
    } while (0)

// https://stackoverflow.com/a/12891181
#ifdef __GNUC__
#define UNUSED(x) UNUSED_##x __attribute__((__unused__))
#else
#define UNUSED(x) UNUSED_##x
#endif

#ifdef __GNUC__
#define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_##x
#else
#define UNUSED_FUNCTION(x) UNUSED_##x
#endif

template <typename T>
bool set_contains(const std::set<T> &set, const T &item) {
    return set.find(item) != set.end();
}

void fatal(const char *fmt, ...);

inline double sign(double x) {
    return x > 0 ? +1 : (x == 0 ? 0 : -1);
}

inline float clip_abs(float x, float y) {
    if (x > y)
        return y;
    if (x < -y)
        return -y;
    return x;
}

inline std::string to_lower(std::string s) {
    auto lc = s;
    transform(lc.begin(), lc.end(), lc.begin(), [](unsigned char c){ return std::tolower(c); }); 
    return lc;
}