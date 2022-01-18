#include "webview_host.h"

#include <future>
#include <iostream>

#include "util/rohelper.h"

// static
std::unique_ptr<WebviewHost> WebviewHost::Create(
    std::optional<std::string> user_data_directory,
    std::optional<std::string> browser_exe_path,
    std::optional<std::string> arguments) {
  wil::com_ptr<CoreWebView2EnvironmentOptions> opts;
  if (arguments.has_value()) {
    opts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    std::wstring warguments(arguments.value().begin(), arguments.value().end());
    opts->put_AdditionalBrowserArguments(warguments.c_str());
  }

  std::optional<std::wstring> user_data_dir;
  if (user_data_directory.has_value()) {
    user_data_dir =
        std::wstring(user_data_directory->begin(), user_data_directory->end());
  }

  std::optional<std::wstring> browser_path;
  if (browser_exe_path.has_value()) {
    browser_path =
        std::wstring(browser_exe_path->begin(), browser_exe_path->end());
  }

  std::promise<HRESULT> result_promise;
  wil::com_ptr<ICoreWebView2Environment> env;
  auto result = CreateCoreWebView2EnvironmentWithOptions(
      browser_path.has_value() ? browser_path->c_str() : nullptr,
      user_data_dir.has_value() ? user_data_dir->c_str() : nullptr, opts.get(),
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [&promise = result_promise, &ptr = env](
              HRESULT r, ICoreWebView2Environment* env) -> HRESULT {
            promise.set_value(r);
            ptr.swap(env);
            return S_OK;
          })
          .Get());

  if (SUCCEEDED(result)) {
    result = result_promise.get_future().get();
    if ((SUCCEEDED(result) || result == RPC_E_CHANGED_MODE) && env) {
      auto webview_env3 = env.try_query<ICoreWebView2Environment3>();
      if (webview_env3) {
        return std::unique_ptr<WebviewHost>(
            new WebviewHost(std::move(webview_env3)));
      }
    }
  }

  return {};
}

WebviewHost::WebviewHost(wil::com_ptr<ICoreWebView2Environment3> webview_env)
    : webview_env_(webview_env) {
  compositor_ = winrt::Windows::UI::Composition::Compositor();
}

void WebviewHost::CreateWebview(HWND hwnd, bool offscreen_only,
                                bool owns_window,
                                WebviewCreationCallback callback) {
  CreateWebViewCompositionController(
      hwnd, [=, self = this](
                wil::com_ptr<ICoreWebView2CompositionController> controller) {
        if (controller) {
          std::unique_ptr<Webview> webview(new Webview(
              std::move(controller), self, hwnd, owns_window, offscreen_only));
          callback(std::move(webview));
        } else {
          callback(nullptr);
        }
      });
}

void WebviewHost::CreateWebViewCompositionController(
    HWND hwnd, CompositionControllerCreationCallback callback) {
  auto success = SUCCEEDED(webview_env_->CreateCoreWebView2CompositionController(
      hwnd,
      Callback<
          ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
          [callback](HRESULT result, ICoreWebView2CompositionController*
                                         compositionController) -> HRESULT {
            if (SUCCEEDED(result)) {
              callback(
                  std::move(wil::com_ptr<ICoreWebView2CompositionController>(
                      compositionController)));
            } else {
              callback(nullptr);
            }

            return S_OK;
          })
          .Get()));

  if (!success) {
    callback(nullptr);
  }
}

winrt::Windows::UI::Composition::Visual WebviewHost::CreateSurface() const {
  return compositor_.CreateContainerVisual();
}
