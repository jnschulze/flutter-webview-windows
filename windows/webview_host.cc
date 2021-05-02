#include "webview_host.h"

#include <wil/com.h>
#include <wrl.h>

#include <future>
#include <iostream>

std::unique_ptr<WebviewHost> WebviewHost::Create(
    std::optional<std::string> arguments) {
  std::promise<HRESULT> result_promise;
  wil::com_ptr<ICoreWebView2Environment> env;

  wil::com_ptr<CoreWebView2EnvironmentOptions> opts;
  if (arguments.has_value()) {
    opts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    std::wstring warguments(arguments.value().begin(), arguments.value().end());
    opts->put_AdditionalBrowserArguments(warguments.c_str());
  }

  auto result = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, opts.get(),
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [&promise = result_promise, &ptr = env](
              HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
            promise.set_value(r);
            ptr.swap(env);
            return S_OK;
          })
          .Get());

  if (result != S_OK || result_promise.get_future().get() != S_OK || !env) {
    return {};
  }

  auto webViewEnvironment3 = env.try_query<ICoreWebView2Environment3>();
  if (!webViewEnvironment3) {
    return {};
  }

  auto host = std::unique_ptr<WebviewHost>(new WebviewHost());
  host->webview_env_ = webViewEnvironment3;

  return host;
}

WebviewHost::WebviewHost() {
  compositor_ = winrt::Windows::UI::Composition::Compositor();
}

void WebviewHost::CreateWebview(HWND hwnd, bool offscreen_only,
                                WebviewCreationCallback callback) {
  std::optional<HWND> view_hwnd;
  if (!offscreen_only) {
    view_hwnd = hwnd;
  }

  CreateWebViewCompositionController(
      hwnd, [callback, view_hwnd, self = this](
                wil::com_ptr<ICoreWebView2CompositionController> controller) {
        std::unique_ptr<Webview> webview(
            new Webview(std::move(controller), self, view_hwnd));
        callback(std::move(webview));
      });
}

void WebviewHost::CreateWebViewCompositionController(
    HWND hwnd, CompositionControllerCreationCallback callback) {
  webview_env_->CreateCoreWebView2CompositionController(
      hwnd,
      Callback<
          ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
          [callback](HRESULT result,
                     ICoreWebView2CompositionController* compositionController)
              -> HRESULT {
            callback(std::move(wil::com_ptr<ICoreWebView2CompositionController>(
                compositionController)));
            return S_OK;
          })
          .Get());
}

winrt::Windows::UI::Composition::Visual WebviewHost::CreateSurface() const {
  return compositor_.CreateContainerVisual();
}
