# Phantom - GTA V Multiplayer Framework (Educational Purpose Only)

> **Disclaimer:**  
> This project is for **educational purposes only**.  
> It is not intended for cheating, piracy, or violating the terms of service of any software.  
> The authors are not responsible for any misuse of this code.

---

## Overview

This project is an experimental framework that demonstrates the technical foundations required to implement a **custom multiplayer system in Grand Theft Auto V (GTA 5)**.

Inspired by popular multiplayer mods (like FiveM, RageMP, and AltV), this repository aims to explore:

- DLL injection techniques
- Game memory manipulation
- Hooking native GTA V functions
- Basic networking concepts to sync players
- Custom entity spawning and world synchronization

---

## Features

- ✅ DLL Injector to inject a custom module into GTA 5
- ✅ Example detours (function hooks) to manipulate in-game logic
- ✅ Basic networking stub for syncing player state
- ✅ Custom player spawning (disabling single-player flow)
- ✅ UI overlay to display multiplayer debug info

---

## Requirements

- Windows 10+
- GTA V (latest supported version — see [Compatibility](#compatibility))
- Visual Studio 2022 (C++ Desktop Development workload)
- Rust (for any modules written in Rust)
- [WinAPI](https://docs.microsoft.com/en-us/windows/win32/apiindex/windows-api-list) knowledge
- [ImGui](https://github.com/ocornut/imgui) for UI overlays

---

## Setup & Build

1. **Clone the repository**

```bash
git clone https://github.com/yourusername/gta5-multiplayer-edu.git
