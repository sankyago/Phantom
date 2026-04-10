# CLAUDE.md — Phantom Project Guide

This document is a comprehensive reference for any AI assistant (or developer) working on the Phantom codebase. It explains the architecture, build system, code conventions, and common tasks.

## What Is Phantom

Phantom is a GTA V multiplayer framework built for educational purposes. The C++ client is injected into GTA V as a DLL, hooks into the game's rendering and scripting systems, and communicates with an async Rust server over WebSockets. The project was rewritten from Rust to C++26 (client side) with a modernized async Rust server.

---

## Project Structure

```
Phantom/
├── Phantom.Hook/                # Static library (.lib) — core hooking engine
├── Phantom.Client/              # Dynamic library (client.dll) — injected into GTA V
├── Phantom.Launcher/            # Executable — DLL injector
├── Phantom.Shared/              # Static library — shared types, FlatBuffers schema
```

### Phantom.Hook (Static Library, .lib)

Core hooking engine providing:

- **Memory abstractions** — `MemoryHandle`, `MemoryRegion`, `Module` for safe memory manipulation
- **PatternScanner** — AOB (array-of-bytes) signature scanning with wildcard support
- **VMTHook** — Virtual method table hooking, auto-restores on destruction
- **DetoursHook** — RAII wrapper around Microsoft Detours for inline function hooks, uses C++ concepts for template constraints
- **D3D11Hook** — Hooks `IDXGISwapChain::Present` at vtable index 8 to inject overlay rendering
- **InputHook** — Subclasses the game window's WndProc to intercept keyboard/mouse input
- **FiberManager** — Windows fiber management for cooperative game script execution
- **NativeInvoker** — Calls GTA V internal (native) functions via the native registration table and crossmap hash translation

### Phantom.Client (Dynamic Library, client.dll)

Injected into GTA V. Contains:

- Game state management
- Entity manager
- Script thread (fiber-based)
- Ultralight HTML/CSS/JS overlay (swappable via `IOverlay` interface)
- WebSocket network client

### Phantom.Launcher (Executable)

DLL injector that:

1. Finds or launches GTA V
2. Injects `client.dll` using `LoadLibraryA` + `CreateRemoteThread`

### Phantom.Shared (Static Library)

- Shared constants and entity types
- FlatBuffers schema (`messages.fbs`) — the source of truth for the network protocol

### server/ (Rust)

Async Tokio runtime server with:

- `tokio-tungstenite` WebSocket server on port 7788
- `SessionManager` — `DashMap`-based player tracking with per-player `Mutex<SplitSink>`
- `EntitySyncBroadcaster` — `DashMap` of entity states, broadcasts updates to all clients
- `MessageRouter` — Placeholder for FlatBuffers message dispatch

### Test Projects

- `Phantom.Hook.Tests/`, `Phantom.Client.Tests/`, `Phantom.Launcher.Tests/` — Google Test + Google Mock
- Server tests use `tokio::test`
- Tests cover: memory abstractions, pattern scanning, VMT hooks, process finding, registry reading

---

## Dependency Graph

```
Phantom.Launcher -> Phantom.Shared
Phantom.Client   -> Phantom.Hook -> Phantom.Shared
Phantom.Client   -> Phantom.Shared
```

All three leaf projects depend on `Phantom.Shared`. `Phantom.Client` depends on both `Phantom.Hook` and `Phantom.Shared`.

---

## Build System

### C++ (MSBuild / Visual Studio 2022)

- **Solution**: `Phantom.sln`
- **Shared properties** in `Directory.Build.props` at repository root:
  - C++26 standard (`/std:c++latest`)
  - No exceptions
  - Warning level W4, treat warnings as errors
  - `WIN32_LEAN_AND_MEAN`, `NOMINMAX` defined
- **Platform**: x64 only
- **Configurations**: Debug, Release
- **PlatformToolset**: v143
- **WindowsTargetPlatformVersion**: 10.0

### Rust Server

```
cd server && cargo build
```

### CI

`.github/workflows/build.yml` runs:

- C++ build on `windows-latest`
- Rust build on `ubuntu-latest`

---

## Architecture and Key Design Decisions

### Error Handling: `std::expected<T, E>`, No Exceptions

All error handling uses `std::expected<T, E>` (C++23/26). Exceptions are disabled project-wide. Every fallible function returns an `expected` type. Never add `throw` or `try`/`catch` to this codebase.

### RAII Everywhere

- `ScopedHandle` wraps Windows `HANDLE` values (auto-closes on destruction)
- `VMTHook` and `DetoursHook` auto-restore original function pointers on destruction
- No manual cleanup calls; destructors handle everything

### C++ Concepts for Template Constraints

`DetoursHook` uses C++ concepts to constrain its template parameter to callable types.

### IOverlay Interface

`Phantom.Client/include/client/overlay/i_overlay.h` defines the overlay abstraction. Ultralight is the current implementation. The interface is designed so that a CEF-based implementation can be swapped in without touching the rest of the client.

### FlatBuffers for Serialization

Cross-language serialization between C++ client and Rust server. The schema lives at `Phantom.Shared/include/shared/messages.fbs` and is the single source of truth for the network protocol.

### WebSockets for Networking

`ixwebsocket` on the client, `tokio-tungstenite` on the server. Communication happens over WebSocket on port 7788.

### Logging

`spdlog` with two sinks: file output and `OutputDebugString` (for viewing in debuggers like Visual Studio or DbgView).

---

## How DLL Injection Works

1. **Find the game**: Launcher uses `CreateToolhelp32Snapshot` to find a running `GTA5.exe`, or launches it from the registry-detected install path.
2. **Inject the DLL**: `OpenProcess` -> `VirtualAllocEx` (allocate memory in target) -> `WriteProcessMemory` (write DLL path string) -> `CreateRemoteThread` targeting `LoadLibraryA`.
3. **DllMain fires**: Inside the GTA V process, `DllMain` (`Phantom.Client/src/dllmain.cpp`) spawns an initialization thread.
4. **Init thread runs**: Hooks D3D11 Present (VMT index 8), hooks WndProc, initializes Ultralight overlay, creates the fiber manager, and connects to the WebSocket server.

---

## How Hooking Works

### PatternScanner

Finds function addresses by scanning process memory for byte signature (AOB) patterns. Supports wildcard bytes (typically `?` or `??`). Used to locate game functions whose addresses change between game updates.

### VMTHook

Replaces entries in a C++ virtual method table. Stores the original pointer and restores it on destruction. Used primarily for hooking the D3D11 swapchain's `Present` method (vtable index 8).

### DetoursHook

RAII wrapper around Microsoft Detours. Creates inline hooks by rewriting the first few bytes of a target function to jump to a detour. Template-constrained via C++ concepts. Automatically unhooks on destruction.

### D3D11Hook

Hooks `IDXGISwapChain::Present` to intercept each frame before it is displayed. This is where the overlay renders its content into the game's frame.

### InputHook

Replaces the game window's WndProc using `SetWindowLongPtr`. Intercepts keyboard and mouse messages before the game processes them. Used for UI interaction with the overlay.

### FiberManager

Uses Windows fibers for cooperative multitasking within the game's script thread. Game scripts yield via fibers, allowing multiple logical scripts to run within a single game thread without blocking.

### NativeInvoker

Calls GTA V's built-in (native) functions. Works by:

1. Looking up the native hash in the crossmap (`crossmap.h`) to translate from known hashes to the current game version's hashes
2. Finding the native handler in the game's registration table
3. Pushing arguments and invoking the handler

Usage: `NativeInvoker::invoke<ReturnType>(hash, args...)`

---

## How the Server Works

- Listens on `0.0.0.0:7788` for WebSocket connections
- `SessionManager` tracks connected players using `DashMap` (concurrent hashmap). Each player has a `Mutex<SplitSink>` for thread-safe message sending
- `EntitySyncBroadcaster` maintains a `DashMap` of entity states and broadcasts updates to all connected clients
- `MessageRouter` is a placeholder for dispatching incoming FlatBuffers messages to appropriate handlers
- Entry point: `server/src/main.rs`

---

## Code Style

### Naming Conventions

| Element               | Convention    | Example                    |
|-----------------------|---------------|----------------------------|
| Functions / variables | `snake_case`  | `find_process`, `hook_ptr` |
| Types / classes       | `PascalCase`  | `MemoryHandle`, `VMTHook`  |
| Constants / macros    | `UPPER_CASE`  | `MAX_PLAYERS`, `NOMINMAX`  |

### Formatting

- `.clang-format` at repository root (Microsoft base style, Allman braces, 120-column limit)
- `.clang-tidy` for static analysis

### Namespaces

- `phantom::hook` — hooking library
- `phantom::client` — client module
- `phantom::launcher` — launcher module
- `phantom` — shared / top-level

---

## Key Files

| File | Purpose |
|------|---------|
| `Phantom.Client/src/dllmain.cpp` | DLL entry point, initialization orchestration |
| `Phantom.Hook/include/hook/memory.h` | `MemoryHandle`, `MemoryRegion`, `Module` |
| `Phantom.Hook/include/hook/natives.h` | `NativeInvoker` — how to call GTA V functions |
| `Phantom.Hook/include/hook/crossmap.h` | Native hash translation (stub, needs real data) |
| `Phantom.Shared/include/shared/messages.fbs` | FlatBuffers schema (source of truth for protocol) |
| `Phantom.Client/include/client/overlay/i_overlay.h` | `IOverlay` interface for future CEF swap |
| `server/src/main.rs` | Rust server entry point |
| `Directory.Build.props` | Shared C++ build settings |
| `.github/workflows/build.yml` | CI pipeline |

---

## Common Tasks

### Add a New Native Call

1. Add the native hash to `Phantom.Hook/include/hook/crossmap.h`
2. Call it via `NativeInvoker::invoke<ReturnType>(hash, args...)`

### Add a New Network Message

1. Update the FlatBuffers schema at `Phantom.Shared/include/shared/messages.fbs`
2. Regenerate FlatBuffers headers (run `flatc` with appropriate flags)
3. Update `NetworkClient` on the C++ side
4. Update the server's `MessageRouter` on the Rust side

### Change the Overlay UI

Edit `Phantom.Client/ui/index.html` (and related CSS/JS). The Ultralight engine renders standard HTML/CSS/JS.

### Add a New Hook

- **Inline hook**: `DetoursHook<FuncType>::create(target, detour)`
- **Vtable hook**: `VMTHook::create(vtable_ptr, index, replacement)`

Both are RAII — store the returned object and the hook stays active for its lifetime.

### Run Tests

- C++: Build and run the test projects in Visual Studio, or use `vstest.console.exe` / `ctest`
- Rust: `cd server && cargo test`

---

## Things to Watch Out For

- **crossmap.h is a stub** — it needs real native hash data for actual GTA V integration
- **No exceptions** — never add `throw`, `try`, or `catch`; use `std::expected` instead
- **x64 only** — the project does not support 32-bit builds
- **Third-party libraries are vendored** in `third_party/` — do not use a package manager for them
- **FlatBuffers schema is the protocol contract** — always update the schema first, then regenerate code for both C++ and Rust
