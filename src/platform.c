#include "platform.h"

#include <string.h>

platform_id platform_from_env(const char *platform_env, const char *device_env) {
    if (platform_env != NULL && strcmp(platform_env, "my355") == 0)
        return PLATFORM_ID_MY355;
    if (platform_env != NULL && strcmp(platform_env, "mac") == 0)
        return PLATFORM_ID_MACOS;
    if (device_env != NULL && strcmp(device_env, "my355") == 0)
        return PLATFORM_ID_MY355;
#if defined(PLATFORM_MY355)
    return PLATFORM_ID_MY355;
#elif defined(PLATFORM_MAC)
    return PLATFORM_ID_MACOS;
#else
    return PLATFORM_ID_UNKNOWN;
#endif
}

const char *platform_name(platform_id id) {
    switch (id) {
        case PLATFORM_ID_MY355:
            return "my355";
        case PLATFORM_ID_MACOS:
            return "mac";
        default:
            return "unknown";
    }
}
