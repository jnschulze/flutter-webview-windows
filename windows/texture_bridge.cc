#include "texture_bridge.h"

#include <windows.foundation.h>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>

#include "util/direct3d11.interop.h"

namespace {
const int kNumBuffers = 1;
const float kPoolHeadroomMultiplier = 1.25f;
}  // namespace

TextureBridge::TextureBridge(GraphicsContext* graphics_context,
                             ABI::Windows::UI::Composition::IVisual* visual)
    : graphics_context_(graphics_context) {
  capture_item_ =
      graphics_context_->CreateGraphicsCaptureItemFromVisual(visual);
  assert(capture_item_);

  capture_item_->add_Closed(
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Graphics::Capture::GraphicsCaptureItem*,
          IInspectable*>>(
          [](ABI::Windows::Graphics::Capture::IGraphicsCaptureItem* item,
             IInspectable* args) -> HRESULT {
            std::cerr << "Capture item was closed." << std::endl;
            return S_OK;
          })
          .Get(),
      &on_closed_token_);
}

TextureBridge::~TextureBridge() {
  const std::lock_guard<std::mutex> lock(mutex_);
  StopInternal();
  if (capture_item_) {
    capture_item_->remove_Closed(on_closed_token_);
  }
}

bool TextureBridge::Start() {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (is_running_ || !capture_item_) {
    return false;
  }

  ABI::Windows::Graphics::SizeInt32 size;
  capture_item_->get_Size(&size);

  ABI::Windows::Graphics::SizeInt32 pool_size;
  pool_size.Width = static_cast<INT32>(size.Width * kPoolHeadroomMultiplier);
  pool_size.Height = static_cast<INT32>(size.Height * kPoolHeadroomMultiplier);

  frame_pool_ = graphics_context_->CreateCaptureFramePool(
      graphics_context_->device(),
      static_cast<ABI::Windows::Graphics::DirectX::DirectXPixelFormat>(
          kPixelFormat),
      kNumBuffers, pool_size);
  assert(frame_pool_);

  pool_size_ = {static_cast<size_t>(pool_size.Width),
                static_cast<size_t>(pool_size.Height)};

  frame_pool_->add_FrameArrived(
      Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Graphics::Capture::Direct3D11CaptureFramePool*,
          IInspectable*>>(
          [this](ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool*
                     pool,
                 IInspectable* args) -> HRESULT {
            OnFrameArrived();
            return S_OK;
          })
          .Get(),
      &on_frame_arrived_token_);

  if (FAILED(frame_pool_->CreateCaptureSession(capture_item_.get(),
                                               capture_session_.put()))) {
    std::cerr << "Creating capture session failed." << std::endl;
    return false;
  }

  if (SUCCEEDED(capture_session_->StartCapture())) {
    is_running_ = true;
    return true;
  }

  return false;
}

void TextureBridge::Stop() {
  const std::lock_guard<std::mutex> lock(mutex_);
  StopInternal();
}

void TextureBridge::StopInternal() {
  if (is_running_) {
    is_running_ = false;
    frame_pool_->remove_FrameArrived(on_frame_arrived_token_);
    auto closable =
        capture_session_.try_as<ABI::Windows::Foundation::IClosable>();
    assert(closable);
    closable->Close();
    capture_session_ = nullptr;
  }
}

void TextureBridge::OnFrameArrived() {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!is_running_) {
    return;
  }

  bool has_frame = false;

  winrt::com_ptr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFrame>
      frame;
  auto hr = frame_pool_->TryGetNextFrame(frame.put());
  if (SUCCEEDED(hr) && frame) {
    winrt::com_ptr<
        ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>
        frame_surface;

    if (SUCCEEDED(frame->get_Surface(frame_surface.put()))) {
      last_frame_ =
          util::TryGetDXGIInterfaceFromObject<ID3D11Texture2D>(frame_surface);
      has_frame = !ShouldDropFrame();
    }

    ABI::Windows::Graphics::SizeInt32 content_size;
    if (SUCCEEDED(frame->get_ContentSize(&content_size))) {
      last_content_size_ = {static_cast<size_t>(content_size.Width),
                            static_cast<size_t>(content_size.Height)};
    }
  }

  if (needs_update_) {
    ABI::Windows::Graphics::SizeInt32 size;
    capture_item_->get_Size(&size);

    // If there's not enough space in the pool, resize it. We give it a bit of
    // extra space to avoid resizing too often, which easily occurs if you are
    // resizing the app window.
    if (static_cast<size_t>(size.Width) > pool_size_.width ||
        static_cast<size_t>(size.Height) > pool_size_.height) {
      ABI::Windows::Graphics::SizeInt32 new_size;
      new_size.Width = static_cast<INT32>(
          (std::max)(size.Width, static_cast<INT32>(pool_size_.width)) *
          kPoolHeadroomMultiplier);
      new_size.Height = static_cast<INT32>(
          (std::max)(size.Height, static_cast<INT32>(pool_size_.height)) *
          kPoolHeadroomMultiplier);

      frame_pool_->Recreate(
          graphics_context_->device(),
          static_cast<ABI::Windows::Graphics::DirectX::DirectXPixelFormat>(
              kPixelFormat),
          kNumBuffers, new_size);

      pool_size_ = {static_cast<size_t>(new_size.Width),
                    static_cast<size_t>(new_size.Height)};
    }

    needs_update_ = false;
  } else if (pool_size_.width > 0 && pool_size_.height > 0) {
    ABI::Windows::Graphics::SizeInt32 size;
    capture_item_->get_Size(&size);

    // If the extra headroom is very large compared to the actual content size,
    // reduce it to save on memory.
    if (pool_size_.width > static_cast<size_t>(size.Width) * 2 ||
        pool_size_.height > static_cast<size_t>(size.Height) * 2) {
      ABI::Windows::Graphics::SizeInt32 new_size;
      new_size.Width = static_cast<INT32>(size.Width * kPoolHeadroomMultiplier);
      new_size.Height =
          static_cast<INT32>(size.Height * kPoolHeadroomMultiplier);

      frame_pool_->Recreate(
          graphics_context_->device(),
          static_cast<ABI::Windows::Graphics::DirectX::DirectXPixelFormat>(
              kPixelFormat),
          kNumBuffers, new_size);

      pool_size_ = {static_cast<size_t>(new_size.Width),
                    static_cast<size_t>(new_size.Height)};
    }
  }

  if (has_frame && frame_available_) {
    frame_available_();
  }
}

bool TextureBridge::ShouldDropFrame() {
  if (!frame_duration_.has_value()) {
    return false;
  }
  auto now = std::chrono::high_resolution_clock::now();

  bool should_drop_frame = false;
  if (last_frame_timestamp_.has_value()) {
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_frame_timestamp_.value());
    should_drop_frame = diff < frame_duration_.value();
  }

  if (!should_drop_frame) {
    last_frame_timestamp_ = now;
  }
  return should_drop_frame;
}

void TextureBridge::NotifySurfaceSizeChanged() {
  const std::lock_guard<std::mutex> lock(mutex_);
  needs_update_ = true;
}

void TextureBridge::SetFpsLimit(std::optional<int> max_fps) {
  const std::lock_guard<std::mutex> lock(mutex_);
  auto value = max_fps.value_or(0);
  if (value != 0) {
    frame_duration_ = FrameDuration(1000.0 / value);
  } else {
    frame_duration_.reset();
    last_frame_timestamp_.reset();
  }
}
