#include "texture_bridge.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>

#include "util/direct3d11.interop.h"
#include "util/swizzle.h"

using namespace winrt::Windows::Graphics::Capture;

namespace {
// corresponds to DXGI_FORMAT_B8G8R8A8_UNORM
const auto kPixelFormat = winrt::Windows::Graphics::DirectX::
    DirectXPixelFormat::B8G8R8A8UIntNormalized;
const int kNumBuffers = 2;
}  // namespace

TextureBridge::TextureBridge(GraphicsContext* graphics_context,
                             winrt::Windows::UI::Composition::Visual surface)
    : graphics_context_(graphics_context) {
  capture_item_ =
      winrt::Windows::Graphics::Capture::GraphicsCaptureItem::CreateFromVisual(
          surface);
  if (!capture_item_) {
    return;
  }

  frame_pool_ = Direct3D11CaptureFramePool::CreateFreeThreaded(
      graphics_context->device(), kPixelFormat, kNumBuffers,
      capture_item_.Size());

  if (!frame_pool_) {
    return;
  }

  capture_session_ = frame_pool_.CreateCaptureSession(capture_item_);
  frame_arrived_ = frame_pool_.FrameArrived(
      winrt::auto_revoke, {this, &TextureBridge::OnFrameArrived});

  is_valid_ = true;
}

bool TextureBridge::Start() {
  if (!is_valid_) {
    return false;
  }
  if (!is_running_) {
    is_running_ = true;
    capture_session_.StartCapture();
  }
  return true;
}

void TextureBridge::OnFrameArrived(
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const&) {
  {
    if (needs_update_) {
      frame_pool_.Recreate(graphics_context_->device(), kPixelFormat,
                           kNumBuffers, capture_item_.Size());
      needs_update_ = false;
    }
  }

  if (frame_available_) {
    frame_available_();
  }
}

void TextureBridge::CopyFrame(ID3D11Texture2D* src_texture) {
  D3D11_TEXTURE2D_DESC desc;
  src_texture->GetDesc(&desc);

  const auto width = desc.Width;
  const auto height = desc.Height;

  bool is_exact_size;
  EnsureStagingTexture(width, height, is_exact_size);

  auto device_context = graphics_context_->d3d_device_context();
  auto staging_texture = staging_texture_.get();

  if (is_exact_size) {
    device_context->CopyResource(staging_texture, src_texture);
  } else {
    D3D11_BOX client_box;
    client_box.top = 0;
    client_box.left = 0;
    client_box.right = width;
    client_box.bottom = height;
    client_box.front = 0;
    client_box.back = 1;
    device_context->CopySubresourceRegion(staging_texture, 0, 0, 0, 0,
                                          src_texture, 0, &client_box);
  }

  D3D11_MAPPED_SUBRESOURCE mappedResource;
  if (device_context->Map(staging_texture, 0, D3D11_MAP_READ, 0,
                          &mappedResource) != S_OK) {
    return;
  }

  if (!pixel_buffer_ || pixel_buffer_->width != width ||
      pixel_buffer_->height != height) {
    if (!pixel_buffer_) {
      pixel_buffer_ = std::make_unique<FlutterDesktopPixelBuffer>();
    }
    pixel_buffer_->width = width;
    pixel_buffer_->height = height;
    const auto size = width * height * 4;
    backing_pixel_buffer_.reset(new uint8_t[size]);
    pixel_buffer_->buffer = backing_pixel_buffer_.get();
  }

  const auto src_pitch_in_pixels = mappedResource.RowPitch / 4;
  RGBA_to_BGRA(reinterpret_cast<uint32_t*>(backing_pixel_buffer_.get()),
               static_cast<const uint32_t*>(mappedResource.pData), height,
               src_pitch_in_pixels, width);

  device_context->Unmap(staging_texture, 0);
}

void TextureBridge::EnsureStagingTexture(uint32_t width, uint32_t height,
                                         bool& is_exact_size) {
  // Only recreate an existing texture if it's too small.
  if (!staging_texture_ || staging_texture_size_.width < width ||
      staging_texture_size_.height < height) {
    D3D11_TEXTURE2D_DESC dstDesc = {};
    dstDesc.ArraySize = 1;
    dstDesc.MipLevels = 1;
    dstDesc.BindFlags = 0;
    dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    dstDesc.Format = static_cast<DXGI_FORMAT>(kPixelFormat);
    dstDesc.Width = width;
    dstDesc.Height = height;
    dstDesc.MiscFlags = 0;
    dstDesc.SampleDesc.Count = 1;
    dstDesc.SampleDesc.Quality = 0;
    dstDesc.Usage = D3D11_USAGE_STAGING;

    staging_texture_ = nullptr;
    if (graphics_context_->d3d_device()->CreateTexture2D(
            &dstDesc, nullptr, staging_texture_.put()) != S_OK) {
      std::cerr << "Creating dst texture failed" << std::endl;
      return;
    }

    staging_texture_size_ = {width, height};
  }

  is_exact_size = staging_texture_size_.width == width &&
                  staging_texture_size_.height == height;
}

const FlutterDesktopPixelBuffer* TextureBridge::CopyPixelBuffer(size_t width,
                                                                size_t height) {
  auto frame = frame_pool_.TryGetNextFrame();
  if (frame) {
    winrt::com_ptr<ID3D11Texture2D> frame_surface =
        GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

    CopyFrame(frame_surface.get());
  }

  return pixel_buffer_.get();
}

void TextureBridge::NotifySurfaceSizeChanged() { needs_update_ = true; }
