#include "lua.h"
#include "lauxlib.h"

#include "lua_syscalls.h"

int lua_window_create(lua_State* L, const lua_os_api_t* api)
{
    const int width = (int)luaL_checkinteger(L, 1);
    const int height = (int)luaL_checkinteger(L, 2);
    const char* title = luaL_optstring(L, 3, "Lua Window");

    if (!api || !api->window_create) {
        /* Windowing syscall is optional on text-only boot targets. */
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, api->window_create(width, height, title));
    return 1;
}
