#ifndef APPS_LUA_APP_H
#define APPS_LUA_APP_H

int lua_app_register_handler(void);
int lua_app_can_handle(const char* path);
int lua_app_launch(const char* path);

#endif /* APPS_LUA_APP_H */
