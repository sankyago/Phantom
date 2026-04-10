#include "hook/d3d11_hook.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace phantom::hook {

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

D3D11Hook* D3D11Hook::instance_ = nullptr;

// ---------------------------------------------------------------------------
// D3D11Hook::create
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<D3D11Hook>, HookError> D3D11Hook::create() {
    // Build a temporary device + swap-chain to grab the Present vtable.
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = ::GetDesktopWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    IDXGISwapChain* tmp_swap = nullptr;
    ID3D11Device* tmp_device = nullptr;
    ID3D11DeviceContext* tmp_context = nullptr;

    const D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr,                  // adapter
        D3D_DRIVER_TYPE_HARDWARE, // driver type
        nullptr,                  // software module
        0,                        // flags
        &feature_level, 1,        // feature levels
        D3D11_SDK_VERSION,
        &sd,
        &tmp_swap,
        &tmp_device,
        nullptr,                  // out feature level
        &tmp_context);

    if (FAILED(hr) || !tmp_swap) {
        if (tmp_context) tmp_context->Release();
        if (tmp_device) tmp_device->Release();
        if (tmp_swap) tmp_swap->Release();
        return std::unexpected(HookError::NullPointer);
    }

    // Hook Present — vtable index 8.
    auto hook_result = VMTHook::create(
        tmp_swap, 8, reinterpret_cast<void*>(&hooked_present));

    // Release temporary objects — we only needed the vtable layout.
    tmp_context->Release();
    tmp_device->Release();
    tmp_swap->Release();

    if (!hook_result) {
        return std::unexpected(hook_result.error());
    }

    auto self = std::unique_ptr<D3D11Hook>(new D3D11Hook());
    self->present_hook_.emplace(std::move(*hook_result));
    instance_ = self.get();

    return self;
}

// ---------------------------------------------------------------------------
// D3D11Hook::add_present_callback
// ---------------------------------------------------------------------------

void D3D11Hook::add_present_callback(PresentCallback callback) {
    present_callbacks_.push_back(std::move(callback));
}

// ---------------------------------------------------------------------------
// D3D11Hook::hooked_present
// ---------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE D3D11Hook::hooked_present(
    IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) {

    if (!instance_) {
        return E_FAIL;
    }

    // On first call, capture the real device and context from the swap-chain.
    if (!instance_->device_) {
        swap_chain->GetDevice(__uuidof(ID3D11Device),
                              reinterpret_cast<void**>(&instance_->device_));
        if (instance_->device_) {
            instance_->device_->GetImmediateContext(&instance_->context_);
        }
    }

    // Invoke all registered present callbacks.
    for (auto& cb : instance_->present_callbacks_) {
        cb(swap_chain, instance_->device_, instance_->context_);
    }

    // Call the original Present.
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    auto original = reinterpret_cast<PresentFn>(
        instance_->present_hook_->original());

    return original(swap_chain, sync_interval, flags);
}

} // namespace phantom::hook
