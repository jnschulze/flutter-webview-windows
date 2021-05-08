#pragma once

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wil/com.h>
#include <winrt/Windows.UI.Composition.h>
#include <wrl.h>

#include <functional>

#include "webview.h"

using namespace Microsoft::WRL;

class WebviewHost {
 public:
  typedef std::function<void(std::unique_ptr<Webview>)> WebviewCreationCallback;
  typedef std::function<void(wil::com_ptr<ICoreWebView2CompositionController>)>
      CompositionControllerCreationCallback;

  static std::unique_ptr<WebviewHost> Create(
      std::optional<std::string> arguments = std::nullopt);

  void CreateWebview(HWND hwnd, bool offscreen_only, bool owns_window,
                     WebviewCreationCallback callback);

  winrt::Windows::UI::Composition::Compositor compositor() const {
    return compositor_;
  }

  winrt::Windows::UI::Composition::Visual CreateSurface() const;

 private:
  winrt::Windows::UI::Composition::Compositor compositor_;
  wil::com_ptr<ICoreWebView2Environment3> webview_env_;

  WebviewHost();
  void CreateWebViewCompositionController(
      HWND hwnd, CompositionControllerCreationCallback cb);
};
