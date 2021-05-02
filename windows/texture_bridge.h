#pragma once

#include <flutter/texture_registrar.h>
#include <windows.graphics.capture.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <cstdint>

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

  bool Start();

  const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width, size_t height);

  void SetOnFrameAvailable(FrameAvailableCallback callback) {
    frame_available_ = std::move(callback);
  }

  void SetOnSurfaceSizeChanged(SurfaceSizeChangedCallback callback) {
    surface_size_changed_ = std::move(callback);
  }

  void NotifySurfaceSizeChanged();

 private:
  bool is_valid_ = false;
  bool is_running_ = false;

  const GraphicsContext* graphics_context_;

  FrameAvailableCallback frame_available_;
  SurfaceSizeChangedCallback surface_size_changed_;

  Size staging_texture_size_ = {0, 0};
  std::atomic<bool> needs_update_ = false;

  winrt::Windows::Graphics::Capture::GraphicsCaptureItem capture_item_{nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool frame_pool_{
      nullptr};

  winrt::Windows::Graphics::Capture::GraphicsCaptureSession capture_session_{
      nullptr};
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::
      FrameArrived_revoker frame_arrived_;

  winrt::com_ptr<ID3D11Texture2D> staging_texture_{nullptr};

  std::unique_ptr<uint8_t> backing_pixel_buffer_;
  std::unique_ptr<FlutterDesktopPixelBuffer> pixel_buffer_;

  void OnFrameArrived(
      winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const&
          sender,
      winrt::Windows::Foundation::IInspectable const&);

  void CopyFrame(ID3D11Texture2D* src_texture);
  void EnsureStagingTexture(uint32_t width, uint32_t height,
                            bool& is_exact_size);
};