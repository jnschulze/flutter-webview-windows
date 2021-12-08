#pragma once

#include "texture_bridge.h"

#include <flutter/texture_registrar.h>

#include "util/direct3d11.interop.h"

class TextureBridgeGpu : public TextureBridge {
 public:
  TextureBridgeGpu(GraphicsContext* graphics_context,
                   winrt::Windows::UI::Composition::Visual visual);

  const FlutterDesktopGpuSurfaceDescriptor* GetSurfaceDescriptor(size_t width,
                                                                 size_t height);

 private:
  FlutterDesktopGpuSurfaceDescriptor surface_descriptor_ = {};
  Size surface_size_ = {0, 0};
  winrt::com_ptr<ID3D11Texture2D> surface_{nullptr};
  winrt::com_ptr<IDXGIResource> dxgi_surface_;

  void ProcessFrame(ID3D11Texture2D* src_texture);
  void EnsureSurface(uint32_t width, uint32_t height, bool& is_exact_size);
};
