#include "client/overlay/ultralight_overlay.h"

#include <Ultralight/Ultralight.h>
#include <AppCore/Platform.h>

namespace phantom::client
{

// ---------------------------------------------------------------------------
// overlay_error_to_string
// ---------------------------------------------------------------------------
[[nodiscard]] const char* overlay_error_to_string(OverlayError error)
{
    switch (error)
    {
    case OverlayError::InitFailed:          return "Overlay initialization failed";
    case OverlayError::RenderFailed:        return "Overlay render failed";
    case OverlayError::DeviceNotAvailable:  return "D3D11 device not available";
    }
    return "Unknown overlay error";
}

// ---------------------------------------------------------------------------
// Pimpl
// ---------------------------------------------------------------------------
struct UltralightOverlay::Impl
{
    ultralight::RefPtr<ultralight::Renderer> renderer;
    ultralight::RefPtr<ultralight::View>     view;
};

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------
UltralightOverlay::UltralightOverlay()
    : impl_(std::make_unique<Impl>())
{
}

UltralightOverlay::~UltralightOverlay()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
std::expected<void, OverlayError> UltralightOverlay::init(HWND hwnd, ID3D11Device* device)
{
    if (!device)
        return std::unexpected(OverlayError::DeviceNotAvailable);

    hwnd_   = hwnd;
    device_ = device;

    // Configure Ultralight platform
    ultralight::Config config;
    config.resource_path_prefix = "./resources/";

    ultralight::Platform::instance().set_config(config);
    ultralight::Platform::instance().set_font_loader(ultralight::GetPlatformFontLoader());
    ultralight::Platform::instance().set_file_system(ultralight::GetPlatformFileSystem("./ui/"));
    ultralight::Platform::instance().set_logger(ultralight::GetDefaultLogger("ultralight.log"));

    // Create renderer
    impl_->renderer = ultralight::Renderer::Create();
    if (!impl_->renderer)
        return std::unexpected(OverlayError::InitFailed);

    // Determine view size from client rect
    RECT rect{};
    ::GetClientRect(hwnd_, &rect);
    uint32_t width  = static_cast<uint32_t>(rect.right  - rect.left);
    uint32_t height = static_cast<uint32_t>(rect.bottom - rect.top);

    if (width == 0 || height == 0)
    {
        width  = 1920;
        height = 1080;
    }

    // Create view and load the overlay HTML
    ultralight::ViewConfig view_config;
    view_config.is_transparent = true;

    impl_->view = impl_->renderer->CreateView(width, height, view_config, nullptr);
    if (!impl_->view)
        return std::unexpected(OverlayError::InitFailed);

    impl_->view->LoadURL("file:///index.html");

    initialized_ = true;
    return {};
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void UltralightOverlay::render()
{
    if (!initialized_ || !visible_)
        return;

    impl_->renderer->Update();
    impl_->renderer->Render();
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------
void UltralightOverlay::shutdown()
{
    if (!initialized_)
        return;

    impl_->view     = nullptr;
    impl_->renderer = nullptr;
    initialized_    = false;
}

// ---------------------------------------------------------------------------
// on_input
// ---------------------------------------------------------------------------
bool UltralightOverlay::on_input(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (!initialized_ || !visible_ || !impl_->view)
        return false;

    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        int x = static_cast<int>(LOWORD(lparam));
        int y = static_cast<int>(HIWORD(lparam));
        ultralight::MouseEvent evt;
        evt.type    = ultralight::MouseEvent::kType_MouseMoved;
        evt.x       = x;
        evt.y       = y;
        evt.button  = ultralight::MouseEvent::kButton_None;
        impl_->view->FireMouseEvent(evt);
        return true;
    }
    case WM_LBUTTONDOWN:
    {
        int x = static_cast<int>(LOWORD(lparam));
        int y = static_cast<int>(HIWORD(lparam));
        ultralight::MouseEvent evt;
        evt.type    = ultralight::MouseEvent::kType_MouseDown;
        evt.x       = x;
        evt.y       = y;
        evt.button  = ultralight::MouseEvent::kButton_Left;
        impl_->view->FireMouseEvent(evt);
        return true;
    }
    case WM_LBUTTONUP:
    {
        int x = static_cast<int>(LOWORD(lparam));
        int y = static_cast<int>(HIWORD(lparam));
        ultralight::MouseEvent evt;
        evt.type    = ultralight::MouseEvent::kType_MouseUp;
        evt.x       = x;
        evt.y       = y;
        evt.button  = ultralight::MouseEvent::kButton_Left;
        impl_->view->FireMouseEvent(evt);
        return true;
    }
    case WM_KEYDOWN:
    {
        ultralight::KeyEvent evt;
        evt.type                = ultralight::KeyEvent::kType_RawKeyDown;
        evt.virtual_key_code    = static_cast<int>(wparam);
        evt.native_key_code     = static_cast<int>(lparam);
        evt.modifiers           = 0;
        impl_->view->FireKeyEvent(evt);
        return true;
    }
    case WM_KEYUP:
    {
        ultralight::KeyEvent evt;
        evt.type                = ultralight::KeyEvent::kType_KeyUp;
        evt.virtual_key_code    = static_cast<int>(wparam);
        evt.native_key_code     = static_cast<int>(lparam);
        evt.modifiers           = 0;
        impl_->view->FireKeyEvent(evt);
        return true;
    }
    default:
        break;
    }

    return false;
}

} // namespace phantom::client
