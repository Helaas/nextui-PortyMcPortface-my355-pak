#ifndef PLATFORM_H
#define PLATFORM_H

typedef enum {
    PLATFORM_ID_UNKNOWN = 0,
    PLATFORM_ID_MY355,
    PLATFORM_ID_MACOS
} platform_id;

platform_id platform_from_env(const char *platform_env, const char *device_env);
const char *platform_name(platform_id id);

#endif
