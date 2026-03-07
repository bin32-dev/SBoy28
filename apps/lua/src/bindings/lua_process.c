#include "lua.h"
#include "lauxlib.h"

#include "lua_syscalls.h"

int lua_process_spawn(lua_State* L, const lua_os_api_t* api)
{
    const char* program = luaL_checkstring(L, 1);

    if (!api || !api->process_spawn) {
        // TODO(OS): process spawning interface.
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, api->process_spawn(program));
    return 1;
}
