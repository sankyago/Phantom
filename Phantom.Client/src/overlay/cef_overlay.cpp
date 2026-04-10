#include "client/overlay/cef_overlay.h"
#include "client/overlay/i_overlay.h"

namespace phantom::client
{

const char* overlay_error_to_string(OverlayError error)
{
    switch (error)
    {
    case OverlayError::InitFailed: return "Overlay initialization failed";
    case OverlayError::RenderFailed: return "Overlay render failed";
    case OverlayError::DeviceNotAvailable: return "D3D11 device not available";
    }
    return "Unknown overlay error";
}

CefOverlay::CefOverlay() = default;
CefOverlay::~CefOverlay() { shutdown(); }

std::expected<void, OverlayError> CefOverlay::init(HWND hwnd, ID3D11Device* device)
{
    if (!device)
        return std::unexpected(OverlayError::DeviceNotAvailable);

    hwnd_ = hwnd;
    device_ = device;

    // TODO: Initialize CEF browser instance in off-screen rendering mode
    // - CefSettings, CefBrowserSettings
    // - Create CefBrowserHost with windowless (OSR) rendering
    // - Set up shared texture for D3D11 compositing

    initialized_ = true;
    return {};
}

void CefOverlay::render()
{
    if (!initialized_ || !visible_)
        return;

    // TODO: Composite CEF's off-screen rendered texture onto the D3D11 backbuffer
}

void CefOverlay::shutdown()
{
    if (!initialized_)
        return;

    // TODO: Destroy CefBrowser, shut down CefApp
    initialized_ = false;
}

bool CefOverlay::on_input(HWND /*hwnd*/, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!initialized_ || !visible_)
        return false;

    // TODO: Forward mouse/keyboard events to CefBrowserHost via SendMouseClickEvent,
    //       SendMouseMoveEvent, SendKeyEvent etc.
    (void)msg;
    (void)wparam;
    (void)lparam;

    return false;
}

} // namespace phantom::client
