cmake_minimum_required(VERSION 3.16)

project(sboy28_lua_module LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckIncludeFile)
check_include_file(assert.h LUA_HAS_HOSTED_ASSERT_H)

set(LUA_MODULE_HEADERS
    include/lua_app.h
    include/lua_runtime.h
    include/lua_syscalls.h
)

set(LUA_MODULE_CORE_SOURCES
    src/core/lua_app.c
    src/utils/lua_utils.c
)

if(LUA_HAS_HOSTED_ASSERT_H)
    set(LUA_MODULE_SOURCES
        ${LUA_MODULE_CORE_SOURCES}
        src/runtime/lua_runtime.c
        src/runtime/lua_loader.c
        src/bindings/lua_syscalls.c
        src/bindings/lua_fs.c
        src/bindings/lua_window.c
        src/bindings/lua_process.c
        src/core/lua_vm.c
    )
else()
    message(WARNING "Hosted C headers are unavailable for the cross toolchain; building Lua runtime stubs.")
    set(LUA_MODULE_SOURCES
        ${LUA_MODULE_CORE_SOURCES}
        src/runtime/lua_runtime_stub.c
    )
endif()

add_library(lua_app_module STATIC
    ${LUA_MODULE_HEADERS}
    ${LUA_MODULE_SOURCES}
)

target_include_directories(lua_app_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(lua_app_module PRIVATE
    -Wall
    -Wextra
    -Wshadow
)


# Integrate with top-level OS build when this directory is included as a subproject.
if (TARGET kernel.elf)
    target_link_libraries(kernel.elf PRIVATE lua_app_module)
endif()
