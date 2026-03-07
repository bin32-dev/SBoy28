# Lua Application Module for SBoy28

## Purpose

`apps/lua` provides a high-level scripting layer for SBoy28 so developers can build applications in Lua instead of writing only low-level C code.

With this module, the OS can:

- initialize and host a Lua VM,
- load `.lua` scripts from the filesystem,
- expose OS services through Lua bindings,
- route Lua scripts through the application manager.

## Directory Structure

```text
apps/lua/
├── include/
│   ├── lua_app.h
│   ├── lua_runtime.h
│   └── lua_syscalls.h
├── src/
│   ├── runtime/
│   │   ├── lua_runtime.c
│   │   └── lua_loader.c
│   ├── bindings/
│   │   ├── lua_syscalls.c
│   │   ├── lua_fs.c
│   │   ├── lua_window.c
│   │   └── lua_process.c
│   ├── core/
│   │   ├── lua_app.c
│   │   └── lua_vm.c
│   └── utils/
│       └── lua_utils.c
├── examples/
│   └── hello.lua
├── CMakeLists.txt
└── README.md
```

The upstream Lua C sources remain in `apps/lua/` and are consumed through `src/core/lua_vm.c`.

## Runtime Flow

1. `lua_runtime_init()` creates a Lua VM and opens standard libraries.
2. `lua_bind_register_syscalls()` creates the Lua `os` table and attaches syscall-backed functions.
3. `lua_runtime_execute(path)` loads and executes the script.
4. `lua_runtime_shutdown()` closes the VM.

## Lua Application Launching

`src/core/lua_app.c` provides app manager hooks:

- `lua_app_can_handle(path)` checks for `.lua` extension.
- `lua_app_launch(path)` executes the script through the runtime.
- `lua_app_register_handler()` is the integration point for OS launcher registration.

Planned command UX:

```text
lua /apps/example.lua
run example.lua
```

## Lua Binding Surface

Exposed API shape from Lua:

```lua
os.print("Hello World")
window = os.window_create(200, 150, "My App")
file = os.fs_open("/home/test.txt")
process = os.process_spawn("app")
```

Current bindings also reserve hooks for memory manager, terminal output, and input polling.

## Kernel / OS Integration TODOs

This module intentionally includes explicit markers where kernel interfaces are required:

- `TODO(OS): implement window creation syscall`
- `TODO(OS): expose filesystem read/write to Lua`
- `TODO(OS): process spawning interface`
- `TODO(OS): implement memory manager stats syscall wrapper`
- `TODO(OS): map terminal output to Lua console API`
- `TODO(OS): expose keyboard/mouse input polling to Lua`
- `TODO(OS): integrate Lua apps with OS launcher`

## Developer Notes

- Add concrete OS syscall glue by calling `lua_syscalls_set_api()` with kernel/app-loader function pointers.
- Register Lua app handling during OS app manager bootstrap.
- Keep new bindings in `src/bindings/` and expand the `os` table in `lua_syscalls.c`.
