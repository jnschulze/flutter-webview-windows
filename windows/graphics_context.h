#pragma once

#include <D3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/windows.graphics.directx.direct3d11.h>

class GraphicsContext {
 public:
  GraphicsContext();

  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device() const { return device_; }
  ID3D11Device* d3d_device() const { return d3dDevice_.get(); }
  ID3D11DeviceContext* d3d_device_context() const { return device_context_.get(); }

 private:
  winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice device_{nullptr};
  winrt::com_ptr<ID3D11Device> d3dDevice_{nullptr};
  winrt::com_ptr<ID3D11DeviceContext> device_context_{nullptr};
};