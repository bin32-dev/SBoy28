# SBoy28

A hobby x86 operating system prototype focused on building a tiny, game-oriented runtime that can eventually host very small Nintendo Switch-era homebrew-style game workloads (targeting payloads under ~1 MB).

## Project Overview

SBoy28 is a 32-bit freestanding OS project that boots directly from a custom boot sector, initializes protected mode, and brings up a simple graphical environment in VGA Mode 13h (320x200, 256 colors).

The long-term objective is to evolve this kernel from a teaching/prototyping OS into a compact application platform capable of running lightweight game and emulator workloads. The repository already includes a Nestopia emulator codebase under `apps/nestopia/` and roadmap docs describing the path toward proper userspace process support.

**Target platform (current implementation):**
- x86 (i686), 32-bit protected mode
- Bootable raw disk image for emulators such as QEMU
- VGA 320x200 graphics output

---

## Implemented Features

### Boot and Kernel Foundation

- **Custom MBR-style boot sector**:
  - Sets video mode to VGA Mode 13h.
  - Enables A20.
  - Reads the kernel from disk sectors into memory.
  - Switches from real mode to protected mode and jumps to kernel entry.
- **Kernel early init path**:
  - GDT initialization (kernel and user descriptors defined).
  - IDT setup with ISR/IRQ entries and PIC remapping.
  - Interrupt handler registration API.

### Memory Subsystems

- **Physical Memory Manager (PMM)**:
  - Bitmap-based frame allocator.
  - Assumes 32 MB memory in current boot flow.
  - Reserves low memory and kernel/bitmap region.
- **Virtual Memory Manager (VMM)**:
  - Paging enabled and identity-mapped for accessible physical memory.
  - Page fault handler registered.
  - Basic map/alloc/free page helpers.
- **Kernel Heap (`kmalloc`/`kfree`)**:
  - Linked-list allocator on top of VMM mappings.
  - 4 MB initial heap region with splitting/merging blocks.

### Threading and Synchronization

- **Kernel threading model**:
  - Round-robin single run queue.
  - Context switching implemented in assembly (`context_switch.S`).
  - Thread create/yield/exit lifecycle.
- **Timer-driven scheduling hook**:
  - PIT initialized at configurable frequency (default used: 100 Hz).
  - IRQ0 calls into scheduler tick path for preemption attempts.
- **Synchronization primitives**:
  - Mutexes (with wait queues).
  - Counting semaphores.
  - Scheduler-level spinlock usage for queue coordination.

### Device Drivers

- **VGA graphics driver**:
  - Backbuffered pixel rendering in Mode 13h.
  - Primitive text drawing via 8x8 font glyphs.
- **PS/2 mouse driver**:
  - IRQ12 handler with 3-byte packet parsing.
  - Position tracking with clamping to 320x200 bounds.
  - Event callback registration.
- **PIT timer driver**:
  - PIT channel 0 setup and interrupt handler binding.
- **RTC driver**:
  - Reads RTC registers and exposes hour/min/sec helpers.
- **Keyboard input (basic)**:
  - Polling-based scancode read path and key buffer.

### UI Functionality in Kernel Demo

`kmain()` currently launches multiple kernel threads that together act as a lightweight interactive UI demo:
- A **time thread** updates minute display from RTC.
- A **mouse render thread** draws and moves a software cursor.
- A **background thread** updates a tick counter.
- Shared drawing is coordinated with a VGA mutex.

### Storage and Loader Experiments

- **FAT-style RAM disk parser** under `src/drivers/iso/`:
  - Boot sector, FAT, root directory parsing helpers.
  - Basic file lookup and read routines.
- **New FAT32 filesystem layer** under `src/drivers/filesystem.c`:
  - Mount FAT32 volumes from a block device buffer.
  - Path-based file open/read/write and seek support.
  - Directory enumeration and FAT cluster allocation for growth.
  - FAT attributes surfaced as file permission bits (e.g., read-only).
- **ELF reader prototype** under `src/drivers/elf/`:
  - Assembly proof-of-concept for opening/reading ELF header bytes.
  - Not integrated into kernel process loading yet.

### Emulator Source Integration

- **Nestopia source is vendored in-repo** (`apps/nestopia/`).
- This is currently a **source integration stage**, not runtime OS integration:
  - Kernel does not yet provide userspace/syscall/runtime compatibility required by Nestopia.
  - Planning documents define required kernel-side work (paging isolation, Ring 3, syscalls, C++ runtime support).

---

## Planned Features / Next Steps

Based on current code and roadmap documents, the next major milestones are:

1. **Process-capable memory model**
   - Per-process page directories (instead of one kernel-global identity map).
   - Safer isolation and userspace address layout.

2. **User mode support (Ring 3)**
   - TSS stack switching path.
   - Controlled transition from kernel mode to user mode tasks.

3. **System call interface**
   - `int 0x80` (or equivalent) dispatcher.
   - Minimal POSIX-like API surface for file I/O, timing, memory mapping.

4. **Executable loading**
   - Integrate ELF/PE loading into kernel process creation path.
   - Segment mapping and entry-point dispatch.

5. **Filesystem architecture**
   - Move from driver-specific logic to VFS abstraction.
   - Unified open/read/write handles across FAT/other backends.

6. **Graphics/audio evolution**
   - Transition from Mode 13h to linear framebuffer path (VESA/GOP-style goals).
   - Add audio backend suitable for emulator/game runtime.

7. **Nestopia compatibility layer**
   - C++ runtime stubs (`new/delete`, global constructors, support glue).
   - Input/video bridge suitable for emulator frame and controller handling.

---

## Technical Details

### Languages and Build Targets

- **Kernel/boot code:** C + x86 assembly
- **Target architecture:** i686 / 32-bit x86
- **Binary format:** freestanding ELF -> raw binaries -> raw disk image

### Toolchain

The top-level build is configured for an ELF cross toolchain:
- `i686-elf-gcc`
- `i686-elf-ld`
- `i686-elf-objcopy`
- CMake 3.16+

### Architectural Choices (current)

- Monolithic kernel style with in-kernel drivers and threads.
- Custom boot flow (not GRUB-dependent for primary image path).
- Identity-mapped paging for simplicity in early bring-up.
- Cooperative + timer-triggered scheduling approach in an evolving preemptive scheduler.
- Backbuffered VGA rendering for deterministic screen updates.

### Emulator Source Notes

- `apps/nestopia/` is a forked/emedded upstream emulator codebase with its own native build systems and dependencies (SDL2/libepoxy/libao/libarchive/zlib in upstream context).
- It is included as integration target/reference while the OS runtime ABI is being implemented.

---

## Build & Run Instructions

## 1) Build SBoy28 kernel image

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Expected artifacts include:
- `bootsector.bin`
- `kernel.bin`
- `SBoy28.img`

## 2) Run in QEMU

From the build directory:

```bash
qemu-system-i386 -drive format=raw,file=SBoy28.img
```

Or use the CMake run target:

```bash
cmake --build . --target run
```

## 3) Windows notes

A Windows-specific build guide is available at:
- `docs/BUILD_WINDOWS.md`

---

## Repository Layout (high level)

- `src/boot/` – boot sector and kernel entry assembly
- `src/kernel/core/` – GDT, IDT, paging, heap, PMM, kernel main
- `src/drivers/` – VGA, RTC, keyboard, PIT, mouse, threading scheduler, storage/ELF experiments
- `src/ui/` – primitive drawing/text utilities
- `include/` – public kernel/driver headers
- `apps/nestopia/` – bundled emulator source tree
- `docs/` – roadmap and integration/build docs

---

## Contribution / Collaboration

Contributions are welcome, especially in:
- Userspace process model and syscall design
- VFS and executable loader integration
- Emulator compatibility shims and runtime support
- Testing/CI for cross-toolchain builds

Recommended contribution style:
- Keep modules small and freestanding-friendly.
- Prefer explicit architecture notes in `docs/` for subsystem changes.
- Include QEMU reproduction steps for new features/bug fixes.
