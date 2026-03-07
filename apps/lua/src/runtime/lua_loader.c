#include "lua.h"
#include "lauxlib.h"

int lua_loader_load_script(lua_State* L, const char* script_path)
{
    if (!L || !script_path) {
        return -1;
    }

    return luaL_loadfile(L, script_path);
}
