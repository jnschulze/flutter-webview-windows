#include "texture_bridge_gpu.h"

#include <iostream>

#include "util/direct3d11.interop.h"

TextureBridgeGpu::TextureBridgeGpu(
    GraphicsContext* graphics_context,
    winrt::Windows::UI::Composition::Visual visual)
    : TextureBridge(graphics_context, visual) {
  surface_descriptor_.format =
      kFlutterDesktopPixelFormatNone;  // no format required for DXGI surfaces
}

void TextureBridgeGpu::ProcessFrame(ID3D11Texture2D* src_texture) {
  D3D11_TEXTURE2D_DESC desc;
  src_texture->GetDesc(&desc);

  const auto width = desc.Width;
  const auto height = desc.Height;

  bool is_exact_size;
  EnsureSurface(width, height, is_exact_size);

  auto device_context = graphics_context_->d3d_device_context();

  if (is_exact_size) {
    device_context->CopyResource(surface_.get(), src_texture);
  } else {
    D3D11_BOX client_box;
    client_box.top = 0;
    client_box.left = 0;
    client_box.right = width;
    client_box.bottom = height;
    client_box.front = 0;
    client_box.back = 1;
    device_context->CopySubresourceRegion(surface_.get(), 0, 0, 0, 0,
                                          src_texture, 0, &client_box);
  }

  device_context->Flush();
}

void TextureBridgeGpu::EnsureSurface(uint32_t width, uint32_t height,
                                     bool& is_exact_size) {
  if (!surface_ || surface_size_.width != width ||
      surface_size_.height != height) {
    D3D11_TEXTURE2D_DESC dstDesc = {};
    dstDesc.ArraySize = 1;
    dstDesc.MipLevels = 1;
    dstDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    dstDesc.CPUAccessFlags = 0;
    dstDesc.Format = static_cast<DXGI_FORMAT>(kPixelFormat);
    dstDesc.Width = width;
    dstDesc.Height = height;
    dstDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    dstDesc.SampleDesc.Count = 1;
    dstDesc.SampleDesc.Quality = 0;
    dstDesc.Usage = D3D11_USAGE_DEFAULT;

    surface_ = nullptr;

    if (graphics_context_->d3d_device()->CreateTexture2D(
            &dstDesc, nullptr, surface_.put()) != S_OK) {
      std::cerr << "Creating intermediate texture failed" << std::endl;
      return;
    }

    HANDLE shared_handle;
    surface_.as(dxgi_surface_);
    dxgi_surface_->GetSharedHandle(&shared_handle);

    surface_descriptor_.handle = shared_handle;
    surface_descriptor_.width = surface_descriptor_.visible_width = width;
    surface_descriptor_.height = surface_descriptor_.visible_height = height;

    surface_size_ = {width, height};
  }

  // surface_descriptor_.visible_width = width;
  // surface_descriptor_.visible_height = height;

  is_exact_size =
      surface_size_.width == width && surface_size_.height == height;
}

const FlutterDesktopGpuSurfaceDescriptor*
TextureBridgeGpu::GetSurfaceDescriptor(size_t width, size_t height) {
  if (!is_running_) {
    return nullptr;
  }

  if (last_frame_) {
    ProcessFrame(last_frame_.get());
  }

  return &surface_descriptor_;
}
