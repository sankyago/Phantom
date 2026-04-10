#pragma once
#include "client/overlay/i_overlay.h"
#include <memory>

namespace phantom::client
{

class CefOverlay : public IOverlay
{
public:
    CefOverlay();
    ~CefOverlay() override;

    CefOverlay(const CefOverlay&) = delete;
    CefOverlay& operator=(const CefOverlay&) = delete;

    [[nodiscard]] std::expected<void, OverlayError> init(HWND hwnd, ID3D11Device* device) override;
    void render() override;
    void shutdown() override;
    [[nodiscard]] bool on_input(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    [[nodiscard]] bool is_visible() const noexcept override { return visible_; }
    void set_visible(bool visible) override { visible_ = visible; }

private:
    HWND hwnd_ = nullptr;
    ID3D11Device* device_ = nullptr;
    bool visible_ = true;
    bool initialized_ = false;
};

} // namespace phantom::client
