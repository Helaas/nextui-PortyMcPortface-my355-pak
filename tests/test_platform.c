#include <assert.h>
#include <string.h>

#include "../src/platform.h"

int main(void) {
    assert(platform_from_env("my355", "my355") == PLATFORM_ID_MY355);
    assert(platform_from_env(NULL, "my355") == PLATFORM_ID_MY355);
    assert(platform_from_env("mac", NULL) == PLATFORM_ID_MACOS);
    assert(strcmp(platform_name(PLATFORM_ID_MY355), "my355") == 0);
    assert(strcmp(platform_name(PLATFORM_ID_MACOS), "mac") == 0);
    return 0;
}
