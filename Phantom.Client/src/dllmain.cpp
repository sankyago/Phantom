// ---------------------------------------------------------------------------
// Phantom.Client — DllMain Orchestration
// ---------------------------------------------------------------------------

#include "client/entities/entity_manager.h"
#include "client/game/game_state.h"
#include "client/game/script_thread.h"
#include "client/net/network_client.h"
#include "client/overlay/cef_overlay.h"
#include "client/overlay/i_overlay.h"

#include "hook/d3d11_hook.h"
#include "hook/fiber.h"
#include "hook/input.h"
#include "hook/memory.h"
#include "hook/natives.h"
#include "hook/pattern_scan.h"

#include "shared/constants.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <memory>
#include <thread>
#include <Windows.h>

namespace {

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

HMODULE g_module = nullptr;

std::unique_ptr<phantom::hook::D3D11Hook>       g_d3d11_hook;
std::unique_ptr<phantom::hook::InputHook>        g_input_hook;
std::unique_ptr<phantom::hook::FiberManager>     g_fiber_manager;
std::unique_ptr<phantom::client::IOverlay> g_overlay;
std::unique_ptr<phantom::client::NetworkClient>  g_network_client;
std::unique_ptr<phantom::client::GameState>      g_game_state;
std::unique_ptr<phantom::client::EntityManager>  g_entity_manager;
std::unique_ptr<phantom::client::ScriptThread>   g_script_thread;

// ---------------------------------------------------------------------------
// setup_logging
// ---------------------------------------------------------------------------

void setup_logging() {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "phantom_client.log", true);
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    auto logger = std::make_shared<spdlog::logger>(
        "phantom",
        spdlog::sinks_init_list{file_sink, msvc_sink});

    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    spdlog::set_default_logger(std::move(logger));
}

// ---------------------------------------------------------------------------
// init_thread
// ---------------------------------------------------------------------------

void init_thread() {
    // 1. Logging
    setup_logging();
    spdlog::info("Phantom v{} initializing...", phantom::PHANTOM_VERSION);

    // 2. D3D11 hook
    auto d3d_result = phantom::hook::D3D11Hook::create();
    if (!d3d_result) {
        spdlog::error("Failed to create D3D11Hook: {}",
                       phantom::hook::hook_error_to_string(d3d_result.error()));
        return;
    }
    g_d3d11_hook = std::move(*d3d_result);
    spdlog::info("D3D11Hook created successfully");

    // 3. Game window + input hook
    HWND game_window = FindWindowA("grcWindow", nullptr);
    if (!game_window) {
        spdlog::error("Failed to find game window (grcWindow)");
        return;
    }

    auto input_result = phantom::hook::InputHook::create(game_window);
    if (!input_result) {
        spdlog::error("Failed to create InputHook: {}",
                       phantom::hook::hook_error_to_string(input_result.error()));
        return;
    }
    g_input_hook = std::move(*input_result);
    spdlog::info("InputHook created successfully");

    // 4. CEF overlay
    g_overlay = std::make_unique<phantom::client::CefOverlay>();
    auto overlay_result = g_overlay->init(game_window, g_d3d11_hook->device());
    if (!overlay_result) {
        spdlog::error("Failed to init CefOverlay");
        return;
    }
    spdlog::info("CefOverlay initialized");

    // 5. Register overlay render in Present callback
    g_d3d11_hook->add_present_callback(
        [](IDXGISwapChain* /*swap_chain*/,
           ID3D11Device* /*device*/,
           ID3D11DeviceContext* /*context*/) {
            g_overlay->render();
        });

    // 6. Forward input to overlay
    g_input_hook->add_callback(
        [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> bool {
            return g_overlay->on_input(hwnd, msg, wparam, lparam);
        });

    // 7. Fiber manager
    auto fiber_result = phantom::hook::FiberManager::create();
    if (!fiber_result) {
        spdlog::error("Failed to create FiberManager: {}",
                       phantom::hook::hook_error_to_string(fiber_result.error()));
        return;
    }
    g_fiber_manager = std::move(*fiber_result);
    spdlog::info("FiberManager created successfully");

    // 8. Network client
    g_network_client = std::make_unique<phantom::client::NetworkClient>();
    auto connect_result = g_network_client->connect("ws://localhost:7788");
    if (!connect_result) {
        spdlog::warn("Failed to connect to server: {}",
                      phantom::client::network_error_to_string(connect_result.error()));
    } else {
        spdlog::info("NetworkClient connected to ws://localhost:7788");
    }

    // 9. Done
    spdlog::info("Phantom client initialization complete");
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void shutdown() {
    g_script_thread.reset();
    g_entity_manager.reset();
    g_game_state.reset();
    g_network_client.reset();
    g_fiber_manager.reset();
    g_overlay.reset();
    g_input_hook.reset();
    g_d3d11_hook.reset();

    spdlog::shutdown();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// DllMain
// ---------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hmodule, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        g_module = hmodule;
        DisableThreadLibraryCalls(hmodule);
        if (HANDLE h = CreateThread(nullptr, 0,
                     [](LPVOID) -> DWORD { init_thread(); return 0; },
                     nullptr, 0, nullptr)) {
            ::CloseHandle(h);
        }
        break;

    case DLL_PROCESS_DETACH:
        shutdown();
        break;

    default:
        break;
    }

    return TRUE;
}
