#pragma once

// Auto-incremented version number
#define VERSION_MAJOR 1
#define VERSION_MINOR 0

// Build number (auto-incremented on each deploy)
#define BUILD_NUMBER 0

// Patch version follows build number
#define VERSION_PATCH BUILD_NUMBER

// Stringify helpers for macro values
#define VERSION_STR_HELPER(x) #x
#define VERSION_STR(x) VERSION_STR_HELPER(x)

// Version string
#define VERSION_STRING "v" VERSION_STR(VERSION_MAJOR) "." VERSION_STR(VERSION_MINOR) "." VERSION_STR(VERSION_PATCH)
