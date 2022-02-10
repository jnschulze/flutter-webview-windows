
#pragma once

#include <inspectable.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <windows.foundation.h>


#include <Windows.graphics.directx.direct3d11.interop.h>

inline auto CreateDirect3DDevice(IDXGIDevice* dxgi_device) {
  winrt::com_ptr<::IInspectable> d3d_device;
  winrt::check_hresult(
      CreateDirect3D11DeviceFromDXGIDevice(dxgi_device, d3d_device.put()));
  return d3d_device
      .as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

inline auto CreateDirect3DSurface(IDXGISurface* dxgi_surface) {
  winrt::com_ptr<::IInspectable> d3d_surface;
  winrt::check_hresult(
      CreateDirect3D11SurfaceFromDXGISurface(dxgi_surface, d3d_surface.put()));
  return d3d_surface
      .as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
}

template <typename T>
auto GetDXGIInterfaceFromObject(
    winrt::Windows::Foundation::IInspectable const& object) {
  auto access = object.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<T> result;
  winrt::check_hresult(
      access->GetInterface(winrt::guid_of<T>(), result.put_void()));
  return result;
}

template <typename T>
auto TryGetDXGIInterfaceFromObject(const winrt::com_ptr<IInspectable>& object) {
  auto access = object.try_as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<T> result;
  access->GetInterface(winrt::guid_of<T>(), result.put_void());
  return result;
}
