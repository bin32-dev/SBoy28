# SBoy28 Developer API Documentation

Welcome to the generated SBoy28 API docs.

## Modules

- **Kernel interfaces**: core types, memory, CPU, drivers.
- **Common helpers**: utility, string/memory helpers, and low-level I/O.
- **Lua app module**: runtime integration and OS syscall surface used by Lua scripts.

## Build documentation

If Doxygen is installed:

```bash
cmake -S . -B build
cmake --build build --target docs
```

Generated HTML will be written to:

- `build/docs/html/index.html`

And XML output will be written to:

- `build/docs/xml/`
