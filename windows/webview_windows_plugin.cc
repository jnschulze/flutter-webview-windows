#include "include/webview_windows/webview_windows_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <windows.h>
#include <wrl.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "graphics_context.h"
#include "util/dispatcher.interop.h"
#include "webview_bridge.h"
#include "webview_host.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowsapp")

using namespace Microsoft::WRL;

namespace {

class WebviewWindowsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  WebviewWindowsPlugin(HWND hwnd, flutter::TextureRegistrar* textures,
                       flutter::BinaryMessenger* messenger);

  virtual ~WebviewWindowsPlugin();

 private:
  std::unordered_map<int64_t, std::unique_ptr<WebviewBridge>> instances_;
  std::unique_ptr<WebviewHost> webview_host_;
  std::unique_ptr<GraphicsContext> graphics_context_;

  winrt::Windows::System::DispatcherQueueController
      dispatcher_queue_controller_{nullptr};

  HWND hwnd_;
  flutter::TextureRegistrar* textures_;
  flutter::BinaryMessenger* messenger_;

  void CreateWebviewInstance(
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>>);
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

// static
void WebviewWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "io.jns.webview.win",
          &flutter::StandardMethodCodec::GetInstance());

  auto hwnd = registrar->GetView()->GetNativeWindow();

  auto plugin = std::make_unique<WebviewWindowsPlugin>(
      hwnd, registrar->texture_registrar(), registrar->messenger());

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

WebviewWindowsPlugin::WebviewWindowsPlugin(HWND hwnd,
                                           flutter::TextureRegistrar* textures,
                                           flutter::BinaryMessenger* messenger)
    : textures_(textures), messenger_(messenger), hwnd_(hwnd) {
  winrt::init_apartment(winrt::apartment_type::single_threaded);
  dispatcher_queue_controller_ = CreateDispatcherQueueController();
}

WebviewWindowsPlugin::~WebviewWindowsPlugin() { instances_.clear(); }

void WebviewWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare("initialize") == 0) {
    return CreateWebviewInstance(std::move(result));
  } else {
    result->NotImplemented();
  }
}

void WebviewWindowsPlugin::CreateWebviewInstance(
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::
          IsSupported()) {
    return result->Error(
        "winrt::Windows::Graphics::Capture::GraphicsCaptureSession is not "
        "supported");
  }

  if (!webview_host_) {
    // webview_host_ = std::move(WebviewHost::Create("--show-fps-counter
    // --ui-show-fps-counter --enable-gpu-rasterization
    // --force-gpu-rasterization
    // --ignore-gpu-blocklist --enable-zero-copy
    // --enable-accelerated-video-decode"));
    webview_host_ = std::move(WebviewHost::Create());
    if (!webview_host_) {
      return result->Error("Creating Webview Host failed");
    }
  }

  if (!graphics_context_) {
    graphics_context_ = std::make_unique<GraphicsContext>();
  }

  std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> holder =
      std::move(result);
  webview_host_->CreateWebview(
      hwnd_, true, [holder, this](std::unique_ptr<Webview> webview) {
        auto bridge = std::make_unique<WebviewBridge>(
            messenger_, textures_, graphics_context_.get(), std::move(webview));
        auto texture_id = bridge->texture_id();
        instances_[texture_id] = std::move(bridge);

        auto response = flutter::EncodableValue(flutter::EncodableMap{
            {flutter::EncodableValue("textureId"),
             flutter::EncodableValue(texture_id)},
        });

        holder->Success(response);
      });
}

}  // namespace

void WebviewWindowsPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  WebviewWindowsPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
