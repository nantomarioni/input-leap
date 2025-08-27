/* Lightweight fork logging implementation. */
#include "base/LogExtension.h"
#include <cstdio>
#include <mutex>
#include <string>
#include <Windows.h>

namespace inputleap {
namespace fork {

static std::mutex s_log_mutex;

static FILE* s_log_file() {
    static FILE* f = nullptr;
    if (f) return f;

    char tempPath[MAX_PATH] = {0};
    DWORD len = GetTempPathA(MAX_PATH, tempPath);
    std::string path;
    if (len > 0 && len < MAX_PATH) {
        path.assign(tempPath);
        if (path.back() != '\\' && path.back() != '/') path.push_back('\\');
    } else {
        path = ".\\";
    }
    path += "inputleap_fork_debug.log";

#ifdef _MSC_VER
    if (fopen_s(&f, path.c_str(), "a") != 0) f = nullptr;
#else
    f = std::fopen(path.c_str(), "a");
#endif
    if (!f) f = stderr;
    return f;
}

void logDebug(const char* fmt, ...) {
    FILE* f = s_log_file();
    std::lock_guard<std::mutex> lock(s_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fflush(f);
}

} // namespace fork
} // namespace inputleap
