# Phantom: Migration to C++26 — Design Spec

## Summary

Full rewrite of the Phantom GTA V multiplayer framework from Rust to C++26. The injection toolchain (launcher, hook, client) moves to C++ with an MSBuild/Visual Studio solution. The server stays in Rust but is modernized with async Tokio. Communication uses WebSockets with FlatBuffers serialization.

## Decision Log

| Decision | Choice | Rationale |
|----------|--------|-----------|
| C++ standard | C++26 | Bleeding edge, leverage latest features |
| Build system | MSBuild / VS solution | Native MSVC, zero abstraction layer |
| Hooking library | Microsoft Detours | Official, MIT licensed, robust |
| UI overlay | Ultralight (swappable to CEF) | Lightweight, GPU-accelerated, HTML/CSS/JS |
| Networking | WebSockets | Simple, bidirectional, easy to debug |
| Serialization | FlatBuffers | Zero-copy, cross-language schema codegen |
| Logging | spdlog | Fast, header-only, community standard |
| Testing | Google Test + Google Mock | Industry standard for C++ |
| Error handling | `std::expected<T, Error>` | No exceptions, explicit error propagation |
| Server runtime | Tokio (async Rust) | Modern async, high performance |

## Solution Structure

```
Phantom.sln
│
├── Phantom.Hook/              # Static library (.lib)
│   ├── include/hook/
│   │   ├── detours.h          # Microsoft Detours RAII wrapper
│   │   ├── pattern_scan.h     # AOB signature scanning
│   │   ├── memory.h           # MemoryHandle, MemoryRegion, Module
│   │   ├── vmt_hook.h         # Virtual Method Table hook
│   │   ├── d3d11_hook.h       # D3D11 Present hook
│   │   ├── input.h            # WndProc keyboard/mouse hook
│   │   ├── natives.h          # GTA V native function invocation
│   │   └── fiber.h            # Windows fiber management
│   ├── src/
│   └── tests/
│
├── Phantom.Client/            # Dynamic library (.dll)
│   ├── include/client/
│   │   ├── overlay/           # IOverlay interface + UltralightOverlay
│   │   ├── entities/          # Vehicle, ped, entity management
│   │   ├── game/              # GameState, cleanup, ScriptThread
│   │   └── net/               # WebSocket client + FlatBuffers messages
│   ├── src/
│   ├── ui/                    # HTML/CSS/JS overlay assets
│   └── tests/
│
├── Phantom.Launcher/          # Executable (.exe)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── injector.cpp
│   │   └── registry.cpp
│   └── tests/
│
├── Phantom.Shared/            # Static library (.lib)
│   ├── include/shared/
│   │   ├── messages.fbs       # FlatBuffers schema (single source of truth)
│   │   ├── entity_types.h
│   │   └── constants.h
│   ├── generated/             # FlatBuffers codegen output
│   └── src/
│
├── server/                    # Rust async server
│   ├── Cargo.toml
│   └── src/
│
├── tests/                     # Solution-level integration tests
├── third_party/               # Vendored: Detours, Ultralight SDK
└── docs/
```

## Component Design

### 1. Phantom.Hook (Static Library)

Core engine for intercepting GTA V functions from inside the process.

**PatternScanner**
- Scans process memory for byte signatures (AOB patterns) to locate functions at runtime
- Supports wildcard bytes (`??`) in patterns
- Returns `std::expected<uintptr_t, ScanError>` on match/failure

**MemoryHandle / MemoryRegion / Module**
- `MemoryHandle`: safe pointer wrapper with arithmetic operators, RIP-relative address resolution
- `MemoryRegion`: represents a contiguous memory region with bounds checking
- `Module`: parses PE headers (DOS/NT) to enumerate exports and sections

**VMTHook**
- RAII class that swaps entries in a Virtual Method Table
- Stores original pointer, restores on destruction
- Used for D3D11 swapchain hooks

**DetoursHook**
- RAII wrapper around Microsoft Detours
- Constructor attaches, destructor detaches
- Template-based to preserve function signature type safety
- Uses concepts to constrain to callable types

**D3D11Hook**
- Hooks `IDXGISwapChain::Present` via VMTHook
- Provides callback registration for render-time code (overlay)
- Captures `ID3D11Device` and `ID3D11DeviceContext` on first call

**InputHook**
- Subclasses game window `WndProc` via `SetWindowLongPtr`
- Intercepts keyboard/mouse input before the game
- Forwards to registered input handlers (overlay, game logic)

**NativeInvoker**
- Locates GTA V's native registration table in memory
- Translates current-version hashes via crossmap to stable hashes
- Invokes natives by pushing arguments to the game's native context

**FiberManager**
- Creates Windows fibers for cooperative multitasking on the game's main thread
- Script registration, per-frame tick, yield support

**Testing:**
- PatternScanner: unit tests with crafted byte buffers
- MemoryHandle/Region/Module: unit tests with mock memory layouts
- VMTHook: tests against fake vtables with known function pointers
- DetoursHook: integration tests hooking known functions in test binaries

### 2. Phantom.Client (DLL)

Injected into GTA V, orchestrates all game-side logic.

**DllMain**
- Entry point, spawns initialization thread to avoid loader lock
- Shutdown handler for clean detach

**GameState**
- Disables singleplayer flow (story mode, loading screens)
- Cleans up unwanted entities on session start
- Manages transition to multiplayer environment

**EntityManager**
- Create/destroy/update vehicles, peds, objects via NativeInvoker
- Tracks spawned entities for cleanup
- Provides typed wrappers: `Vehicle`, `Ped`, `Object`

**ScriptThread**
- Registers with FiberManager as a game script
- Ticks every frame on the main game thread
- Drives game logic, entity updates, input handling

**NetworkClient**
- WebSocket connection to server (IXWebSocket or similar lightweight lib)
- Sends/receives FlatBuffers messages
- Handles connect, disconnect, reconnect with backoff
- Runs on a dedicated thread, posts events to ScriptThread via lock-free queue

**Overlay (abstracted for future CEF swap)**
```cpp
class IOverlay {
public:
    virtual ~IOverlay() = default;
    virtual std::expected<void, OverlayError> init(HWND hwnd, ID3D11Device* device) = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;
    virtual bool on_input(const MSG& msg) = 0;
};
```
- `UltralightOverlay`: implements `IOverlay` using Ultralight SDK
- Renders HTML/CSS/JS from `ui/` directory into a D3D11 texture
- Composited during D3D11Hook's Present callback
- Swapping to CEF = implement `CEFOverlay` against same interface

**Initialization order:**
1. DllMain → spawn init thread
2. Wait for D3D11 device availability
3. Hook D3D11 Present + WndProc
4. Initialize overlay
5. Start script thread (fiber)
6. Connect to server

**Testing:**
- NetworkClient: tested against local WebSocket echo server
- EntityManager: tested with mocked NativeInvoker
- Overlay: tested via IOverlay with stub implementation
- GameState: tested with mocked native calls

### 3. Phantom.Launcher (Executable)

Standalone injector that puts the client DLL into GTA V.

**ProcessFinder**
- Enumerates running processes via `CreateToolhelp32Snapshot`
- Finds `GTA5.exe` by name
- Returns `std::expected<DWORD, FindError>` (process ID or error)

**RegistryHelper**
- Reads GTA V install path from Windows registry
- Checks Rockstar, Steam, and Epic Games Store registry keys
- Returns `std::expected<std::filesystem::path, RegistryError>`

**Injector**
1. `OpenProcess` with `PROCESS_ALL_ACCESS`
2. `VirtualAllocEx` — allocate memory in target process
3. `WriteProcessMemory` — write DLL path string
4. `CreateRemoteThread` targeting `LoadLibraryA`
5. `WaitForSingleObject` — wait for injection completion
6. Verify DLL loaded successfully

**CLI interface:**
- `--launch` — start GTA V then inject
- `--attach` — inject into already running GTA V
- `--verbose` — enable detailed logging

**Testing:**
- ProcessFinder: tested with mock process snapshot
- RegistryHelper: tested with mock registry keys
- Injector: integration test against a dummy target process

### 4. Phantom.Shared (Static Library)

Cross-project definitions shared between C++ components and the Rust server.

**FlatBuffers schema (`messages.fbs`):**
- `EntityUpdate`: position, rotation, model, entity type
- `PlayerConnect` / `PlayerDisconnect`: session management
- `ChatMessage`: text, sender ID
- `WorldState`: full state snapshot for late joiners

**Entity types:**
- `EntityTransform`: position (vec3), rotation (vec3)
- `EntityModel`: model hash, entity type enum

**Constants:**
- Server default port, tick rate, protocol version

**Codegen:**
- FlatBuffers compiler generates C++ headers into `generated/`
- Same `.fbs` files used by Rust server via `flatbuffers` crate

### 5. Server (Rust, Modernized)

Async rewrite of the existing Rust server.

**Dependencies:**
- `tokio` — async runtime
- `tokio-tungstenite` — WebSocket server
- `flatbuffers` — deserialize/serialize from shared `.fbs` schemas
- `tracing` + `tracing-subscriber` — structured async-aware logging
- `dashmap` — concurrent player session map

**Components:**
- **SessionManager**: tracks connected players, assigns IDs
- **MessageRouter**: deserializes incoming FlatBuffers, dispatches to handlers
- **EntitySyncBroadcaster**: receives entity updates, relays to all other connected clients
- **WorldStateManager**: maintains authoritative world state, sends snapshots to new joiners

**Testing:**
- `tokio::test` async test suite
- Mock WebSocket clients for session lifecycle tests
- FlatBuffers round-trip tests against C++ client messages

## Cross-Cutting Concerns

### C++26 Features
- `std::expected<T, E>` for all error handling (no exceptions)
- `std::print` / `std::println` where appropriate
- Concepts for template constraints (hook types, callable signatures)
- Structured bindings throughout
- `constexpr` and `consteval` where applicable
- Modules where MSVC support is stable

### Error Handling
- No exceptions anywhere in the codebase
- All fallible operations return `std::expected<T, Error>`
- RAII for all Windows handles, COM objects, memory allocations
- Custom `ScopedHandle` wrapper for `HANDLE` types

### Logging
- spdlog with two sinks: rotating file + `OutputDebugString`
- Log levels: trace, debug, info, warn, error
- Structured format: `[timestamp] [level] [component] message`

### Code Style
- `snake_case` for functions and variables
- `PascalCase` for types and classes
- `UPPER_CASE` for constants and macros
- `.clang-format` in repo root (BasedOnStyle: Microsoft, modified)
- `.clang-tidy` for static analysis

### Testing Strategy
- Google Test + Google Mock for all C++ tests
- One test project per VS project (Phantom.Hook.Tests, etc.)
- Unit tests: mocked dependencies, fast, run on every build
- Integration tests: solution-level, test cross-component interaction
- CI runs all tests on every push

### CI/CD
- GitHub Actions on `windows-latest`
- MSVC 2022 with `/std:c++latest` flag
- Steps: build → test → clippy (Rust server) → rustfmt (Rust server)
- Separate jobs for C++ and Rust to run in parallel

## Third-Party Dependencies

| Dependency | Version | Purpose | Location |
|-----------|---------|---------|----------|
| Microsoft Detours | 4.x | Function hooking | third_party/detours/ |
| Ultralight | Latest | HTML/CSS/JS overlay | third_party/ultralight/ |
| spdlog | 1.x | Logging | NuGet or third_party/ |
| Google Test | Latest | Testing framework | NuGet or third_party/ |
| FlatBuffers | Latest | Serialization codegen | third_party/flatbuffers/ |
| IXWebSocket (or equivalent) | Latest | WebSocket client (C++) | third_party/ixwebsocket/ |

## Out of Scope

- Full multiplayer gameplay (this is infrastructure)
- Anti-cheat or security hardening
- Voice chat
- Mod loading/plugin system
- Linux/macOS support
