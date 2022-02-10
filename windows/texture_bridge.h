#pragma once

#include <windows.graphics.capture.h>

#include <wrl.h>

#include <cstdint>
#include <functional>

#include "graphics_context.h"

typedef struct {
  size_t width;
  size_t height;
} Size;

class TextureBridge {
 public:
  typedef std::function<void()> FrameAvailableCallback;
  typedef std::function<void(Size size)> SurfaceSizeChangedCallback;

  TextureBridge(GraphicsContext* graphics_context,
                ABI::Windows::UI::Composition::IVisual* visual);
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

  winrt::com_ptr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>
      capture_item_;
  winrt::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool>
      frame_pool_;
  winrt::com_ptr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession>
      capture_session_;

  EventRegistrationToken on_closed_token_ = {};
  EventRegistrationToken on_frame_arrived_token_ = {};

  void OnFrameArrived();

  // corresponds to DXGI_FORMAT_B8G8R8A8_UNORM
  static constexpr auto kPixelFormat = ABI::Windows::Graphics::DirectX::
      DirectXPixelFormat::DirectXPixelFormat_B8G8R8A8UIntNormalized;
};
