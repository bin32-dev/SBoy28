#include "lua_runtime.h"

int lua_runtime_init(void)
{
    return 0;
}

int lua_runtime_execute(const char* script_path)
{
    (void)script_path;
    return -1;
}

int lua_runtime_execute_startup(void)
{
    return -1;
}

void lua_runtime_shutdown(void)
{
}
