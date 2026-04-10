#pragma once

#include "hook/detours_hook.h"

#include <atomic>
#include <d3d11.h>
#include <dxgi.h>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// PresentCallback
// ---------------------------------------------------------------------------

using PresentCallback =
    std::function<void(IDXGISwapChain*, ID3D11Device*, ID3D11DeviceContext*)>;

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

// ---------------------------------------------------------------------------
// D3D11Hook
// ---------------------------------------------------------------------------

class D3D11Hook {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<D3D11Hook>, HookError>
    create();

    ~D3D11Hook();

    D3D11Hook(const D3D11Hook&) = delete;
    D3D11Hook& operator=(const D3D11Hook&) = delete;

    void add_present_callback(PresentCallback callback);

    [[nodiscard]] ID3D11Device* device() const noexcept { return device_; }
    [[nodiscard]] ID3D11DeviceContext* context() const noexcept { return context_; }

private:
    D3D11Hook() = default;

    static HRESULT STDMETHODCALLTYPE hooked_present(
        IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

    std::optional<DetoursHook<PresentFn>> present_hook_;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    std::vector<PresentCallback> present_callbacks_;

    static std::atomic<D3D11Hook*> instance_;
};

} // namespace phantom::hook
