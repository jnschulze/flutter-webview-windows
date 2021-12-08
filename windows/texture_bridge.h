#pragma once

#include <windows.graphics.capture.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <cstdint>
#include <functional>
#include <mutex>

#include "graphics_context.h"
#include "util/capture.desktop.interop.h"

using namespace winrt::Windows::Graphics::Capture;

typedef struct {
  size_t width;
  size_t height;
} Size;

class TextureBridge {
 public:
  typedef std::function<void()> FrameAvailableCallback;
  typedef std::function<void(Size size)> SurfaceSizeChangedCallback;

  TextureBridge(GraphicsContext* graphics_context,
                winrt::Windows::UI::Composition::Visual surface);
  virtual ~TextureBridge();

  bool Start();
  void Stop();

  void SetOnFrameAvailable(FrameAvailableCallback callback) {
    frame_available_ = std::move(callback);
  }

  void SetOnSurfaceSizeChanged(SurfaceSizeChangedCallback callback) {
    surface_size_changed_ = std::move(callback);
  }

  void NotifySurfaceSizeChanged();

 protected:
  bool is_running_ = false;

  const GraphicsContext* graphics_context_;

  FrameAvailableCallback frame_available_;
  SurfaceSizeChangedCallback surface_size_changed_;
  std::atomic<bool> needs_update_ = false;
  winrt::com_ptr<ID3D11Texture2D> last_frame_;

  winrt::Windows::Graphics::Capture::GraphicsCaptureItem capture_item_{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{
      nullptr};

  winrt::Windows::Graphics::Capture::GraphicsCaptureSession capture_session_{
      nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::
      FrameArrived_revoker frame_arrived_;
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem::Closed_revoker
      closed_;

  void OnFrameArrived(
      winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const&
          sender,
      winrt::Windows::Foundation::IInspectable const&);

  void OnClosed(winrt::Windows::Graphics::Capture::GraphicsCaptureItem const&,
                winrt::Windows::Foundation::IInspectable const&);

  // corresponds to DXGI_FORMAT_B8G8R8A8_UNORM
  static constexpr auto kPixelFormat = winrt::Windows::Graphics::DirectX::
      DirectXPixelFormat::B8G8R8A8UIntNormalized;
};
