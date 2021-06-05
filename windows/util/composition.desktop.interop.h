#pragma once

#include <windows.ui.composition.interop.h>

namespace util {

auto CreateDesktopWindowTarget(
    winrt::Windows::UI::Composition::Compositor const& compositor,
    HWND window) {
  namespace abi = ABI::Windows::UI::Composition::Desktop;

  auto interop = compositor.as<abi::ICompositorDesktopInterop>();
  winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget target{nullptr};
  winrt::check_hresult(interop->CreateDesktopWindowTarget(
      window, true,
      reinterpret_cast<abi::IDesktopWindowTarget**>(winrt::put_abi(target))));
  return target;
}

}