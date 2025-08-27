/* Lightweight fork logging implementation for POSIX platforms.
 * Modified to avoid heavier C++ stdlib features on older macOS SDKs.
 */
#include "base/LogExtension.h"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <pthread.h>
#include <limits.h>
#include <string.h>

namespace inputleap {
namespace fork {

static pthread_mutex_t s_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static FILE* s_log_file(void) {
    static FILE* f = NULL;
    if (f) return f;

    const char* tmp = getenv("TMPDIR");
    char path[PATH_MAX];
    path[0] = '\0';

    if (tmp && *tmp) {
        size_t len = strnlen(tmp, PATH_MAX - 1);
        if (len > 0) {
            /* copy and ensure trailing slash */
            strncpy(path, tmp, PATH_MAX - 1);
            path[PATH_MAX - 1] = '\0';
            if (path[len - 1] != '/') {
                if (len + 1 < PATH_MAX) strncat(path, "/", PATH_MAX - strlen(path) - 1);
            }
        }
    }

    if (path[0] == '\0') {
        strncpy(path, "/tmp/", PATH_MAX - 1);
        path[PATH_MAX - 1] = '\0';
    }

    strncat(path, "inputleap_fork_debug.log", PATH_MAX - strlen(path) - 1);

    f = fopen(path, "a");
    if (!f) f = stderr;
    return f;
}

void logDebug(const char* fmt, ...) {
    FILE* f = s_log_file();
    pthread_mutex_lock(&s_log_mutex);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fflush(f);
    pthread_mutex_unlock(&s_log_mutex);
}

} // namespace fork
} // namespace inputleap
