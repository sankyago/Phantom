# Phantom - GTA V Multiplayer Framework (Educational Purpose Only)

> **Disclaimer:**  
> This project is for **educational purposes only**.  
> It is not intended for cheating, piracy, or violating the terms of service of any software.  
> The authors are not responsible for any misuse of this code.

---

## Overview

Phantom is an experimental framework that demonstrates the technical foundations required to implement a **custom multiplayer system in Grand Theft Auto V**.

Inspired by popular multiplayer mods (like FiveM, RageMP, and AltV), this repository explores:

- DLL injection techniques
- Game memory manipulation and pattern scanning
- Hooking native GTA V functions (VMT hooks, Microsoft Detours)
- D3D11 overlay rendering (Ultralight HTML/CSS/JS)
- WebSocket networking with FlatBuffers serialization
- Async multiplayer server architecture

---

## Architecture

The project consists of a **C++26 injection toolchain** and an **async Rust server**:

| Component | Language | Type | Purpose |
|-----------|----------|------|---------|
| **Phantom.Hook** | C++26 | Static lib | Core hooking engine: memory abstractions, pattern scanning, VMT/Detours hooks, D3D11 hook, input hook, fiber manager, native invoker |
| **Phantom.Client** | C++26 | DLL (`client.dll`) | Injected into GTA V: game state, entity management, script thread, Ultralight overlay, WebSocket client |
| **Phantom.Launcher** | C++26 | Executable | DLL injector: finds/launches GTA V, injects client.dll |
| **Phantom.Shared** | C++26 | Static lib | Shared constants, entity types, FlatBuffers schema |
| **Server** | Rust | Binary | Async Tokio WebSocket server with player sessions and entity sync |

### Dependency Graph

```
Phantom.Launcher --> Phantom.Shared
Phantom.Client  --> Phantom.Hook --> Phantom.Shared
Phantom.Client  --> Phantom.Shared
```

---

## Requirements

- Windows 10+ (x64)
- Visual Studio 2022 (C++ Desktop Development workload, v143 toolset)
- Rust toolchain (for the server)
- Third-party dependencies (see below)

### Third-Party Dependencies

| Dependency | Purpose |
|-----------|---------|
| [Microsoft Detours](https://github.com/microsoft/Detours) | Inline function hooking |
| [Ultralight](https://ultralig.ht/) | HTML/CSS/JS overlay rendering |
| [spdlog](https://github.com/gabime/spdlog) | Logging |
| [Google Test](https://github.com/google/googletest) | C++ testing framework |
| [FlatBuffers](https://google.github.io/flatbuffers/) | Cross-language serialization |
| [IXWebSocket](https://github.com/nicholaskrieger/IXWebSocket) | WebSocket client (C++) |

---

## Build

### C++ (Client, Hook, Launcher)

1. Clone the repository
2. Place third-party dependencies in the appropriate include/lib paths (see `Directory.Build.props` and each `.vcxproj` for expected paths)
3. Open `Phantom.sln` in Visual Studio 2022
4. Build in **Debug|x64** or **Release|x64**

### Rust Server

```bash
cd server
cargo build --release
```

---

## Usage

1. Start the server:
   ```bash
   cd server
   cargo run --release
   ```

2. Launch GTA V and inject:
   ```bash
   # Launch GTA V and inject automatically
   Phantom.Launcher.exe --launch

   # Or inject into an already running GTA V
   Phantom.Launcher.exe --attach
   ```

---

## Project Structure

```
Phantom.sln
├── Phantom.Hook/              # Static lib — hooking engine
│   ├── include/hook/          # Public headers
│   └── src/                   # Implementation
├── Phantom.Client/            # DLL — injected into GTA V
│   ├── include/client/        # Public headers
│   ├── src/                   # Implementation
│   └── ui/                    # HTML/CSS/JS overlay assets
├── Phantom.Launcher/          # Executable — DLL injector
│   ├── include/launcher/
│   └── src/
├── Phantom.Shared/            # Static lib — shared types
│   ├── include/shared/
│   └── src/
├── Phantom.Hook.Tests/        # Google Test suite
├── Phantom.Client.Tests/
├── Phantom.Launcher.Tests/
├── server/                    # Rust async server
│   └── src/
├── Directory.Build.props      # Shared C++26 build settings
├── .clang-format              # Code formatting rules
├── .clang-tidy                # Static analysis config
└── CLAUDE.md                  # Detailed project documentation
```

---

## Key Technical Details

- **C++26** with `/std:c++latest` — uses `std::expected`, concepts, structured bindings
- **No exceptions** — all error handling via `std::expected<T, Error>`
- **RAII everywhere** — ScopedHandle, VMTHook, DetoursHook auto-cleanup
- **Overlay is swappable** — `IOverlay` interface allows replacing Ultralight with CEF
- **FlatBuffers schema** at `Phantom.Shared/include/shared/messages.fbs` is the protocol source of truth
- **Crossmap** (`hook/crossmap.h`) translates GTA V native hashes between game versions — currently a stub

---

## License

This project is provided as-is for educational purposes. See individual third-party dependency licenses for their terms.
