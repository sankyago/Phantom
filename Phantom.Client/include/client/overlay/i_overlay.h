#pragma once
#include <expected>
#include <string>
#include <Windows.h>
#include <d3d11.h>

namespace phantom::client
{
enum class OverlayError { InitFailed, RenderFailed, DeviceNotAvailable };
[[nodiscard]] const char* overlay_error_to_string(OverlayError error);

class IOverlay
{
  public:
    virtual ~IOverlay() = default;
    [[nodiscard]] virtual std::expected<void, OverlayError> init(HWND hwnd, ID3D11Device* device) = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;
    [[nodiscard]] virtual bool on_input(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
    [[nodiscard]] virtual bool is_visible() const noexcept = 0;
    virtual void set_visible(bool visible) = 0;
};
} // namespace phantom::client
