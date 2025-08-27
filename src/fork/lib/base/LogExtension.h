/* Lightweight fork logging utility.
 * Provides LOG_DEBUG(...) macro that writes to a per-user file without
 * depending on the project's core logging headers.
 */
#pragma once

#include <cstdarg>

namespace inputleap {
namespace fork {

// Write a formatted debug line to the fork log. Thread-safe.
void logDebug(const char* fmt, ...);

} // namespace fork
} // namespace inputleap

#ifndef LOG_DEBUG
#define LOG_DEBUG(...) inputleap::fork::logDebug(__VA_ARGS__)
#endif
