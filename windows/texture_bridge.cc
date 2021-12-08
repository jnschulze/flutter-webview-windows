#include "texture_bridge.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>

#include "util/direct3d11.interop.h"

using namespace winrt::Windows::Graphics::Capture;

namespace {
const int kNumBuffers = 2;
}  // namespace

TextureBridge::TextureBridge(GraphicsContext* graphics_context,
                             winrt::Windows::UI::Composition::Visual surface)
    : graphics_context_(graphics_context) {
  capture_item_ =
      winrt::Windows::Graphics::Capture::GraphicsCaptureItem::CreateFromVisual(
          surface);

  closed_ = capture_item_.Closed(winrt::auto_revoke,
                                 {this, &TextureBridge::OnClosed});
}

TextureBridge::~TextureBridge() { Stop(); }

bool TextureBridge::Start() {
  if (is_running_ || !capture_item_) {
    return false;
  }

  frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
      graphics_context_->device(), kPixelFormat, kNumBuffers,
      capture_item_.Size());

  frame_arrived_ = frame_pool_.FrameArrived(
      winrt::auto_revoke, {this, &TextureBridge::OnFrameArrived});

  capture_session_ = frame_pool_.CreateCaptureSession(capture_item_);
  is_running_ = true;
  capture_session_.StartCapture();

  return true;
}

void TextureBridge::Stop() {
  if (is_running_) {
    is_running_ = false;
    capture_session_.Close();
    capture_session_ = nullptr;
  }
}

void TextureBridge::OnClosed(
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem const&,
    winrt::Windows::Foundation::IInspectable const&) {}

void TextureBridge::OnFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&) {
  if (!is_running_) {
    return;
  }

  auto frame = frame_pool_.TryGetNextFrame();
  if (frame) {
    winrt::com_ptr<ID3D11Texture2D> frame_surface =
        GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    last_frame_ = frame_surface;
  }

  if (needs_update_) {
    frame_pool_.Recreate(graphics_context_->device(), kPixelFormat, 1,
                         capture_item_.Size());
    needs_update_ = false;
  }

  if (frame_available_) {
    frame_available_();
  }
}

void TextureBridge::NotifySurfaceSizeChanged() { needs_update_ = true; }
