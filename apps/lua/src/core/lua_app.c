#include "lua_app.h"
#include "lua_runtime.h"

#include <stddef.h>
#include <string.h>

static int has_lua_extension(const char* path)
{
    const size_t len = strlen(path);

    if (len < 4) {
        return 0;
    }

    return strcmp(path + len - 4, ".lua") == 0;
}

int lua_app_register_handler(void)
{
    // TODO(OS): integrate Lua apps with OS launcher registration table.
    // TODO(OS): route run <script.lua> and lua <script.lua> commands to Lua runtime.
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
