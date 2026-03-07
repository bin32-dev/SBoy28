#ifndef APPS_LUA_RUNTIME_H
#define APPS_LUA_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

int lua_runtime_init(void);
int lua_runtime_execute(const char* script_path);
void lua_runtime_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* APPS_LUA_RUNTIME_H */
