#include "lua_app.h"
#include "lua_runtime.h"

#include <stddef.h>
#include <string.h>

static size_t local_strlen(const char* str)
{
    size_t len = 0;
    while (str && str[len] != '\0') {
        ++len;
    }
    return len;
}

static int has_lua_extension(const char* path)
{
    const size_t len = local_strlen(path);

    if (len < 4) {
        return 0;
    }

    return path[len - 4] == '.' &&
           path[len - 3] == 'l' &&
           path[len - 2] == 'u' &&
           path[len - 1] == 'a';
}

int lua_app_register_handler(void)
{
    /*
     * Launcher integration point: the shell can check lua_app_can_handle()
     * and dispatch to lua_app_launch().
     */
    return 0;
}

int lua_app_can_handle(const char* path)
{
    if (!path) {
        return 0;
    }

    return has_lua_extension(path);
}

int lua_app_launch(const char* path)
{
    if (!lua_app_can_handle(path)) {
        return -1;
    }

    return lua_runtime_execute(path);
}
