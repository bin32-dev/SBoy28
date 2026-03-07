#include "lua.h"
#include "lauxlib.h"

#include "lua_syscalls.h"

#include <stddef.h>

int lua_fs_open(lua_State* L, const lua_os_api_t* api)
{
    const char* path = luaL_checkstring(L, 1);

    if (!api || !api->fs_open) {
        // TODO(OS): expose filesystem read/write to Lua via syscall table.
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, api->fs_open(path));
    return 1;
}
