#pragma once

#include "texture_bridge.h"

#include <flutter/texture_registrar.h>

class TextureBridgeFallback : public TextureBridge {
 public:
  TextureBridgeFallback(GraphicsContext* graphics_context,
                        winrt::Windows::UI::Composition::Visual surface);

  const FlutterDesktopPixelBuffer* CopyPixelBuffer(size_t width, size_t height);

 private:
  Size staging_texture_size_ = {0, 0};
  winrt::com_ptr<ID3D11Texture2D> staging_texture_{nullptr};
  std::unique_ptr<uint8_t> backing_pixel_buffer_;
  std::unique_ptr<FlutterDesktopPixelBuffer> pixel_buffer_;

  void ProcessFrame(ID3D11Texture2D* src_texture);
  void EnsureStagingTexture(uint32_t width, uint32_t height,
                            bool& is_exact_size);
};
