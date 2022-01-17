#include "include/webview_windows/webview_windows_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <shlobj.h>
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "graphics_context.h"
#include "webview_bridge.h"
#include "webview_host.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowsapp")

namespace {

constexpr auto kMethodInitialize = "initialize";
constexpr auto kMethodDispose = "dispose";
constexpr auto kMethodInitializeEnvironment = "initializeEnvironment";
constexpr auto kErrorMessageEnvironmentCreationFailed =
    "Creating Webview environment failed";

static std::optional<std::string> GetDefaultDataDirectory() {
  PWSTR path_tmp;
  if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr,
                                      &path_tmp))) {
    return std::nullopt;
  }
  auto path = std::filesystem::path(path_tmp);
  CoTaskMemFree(path_tmp);

  wchar_t filename[MAX_PATH];
  GetModuleFileName(nullptr, filename, MAX_PATH);
  path /= "flutter_webview_windows";
  path /= std::filesystem::path(filename).stem();

  return path.string();
}

template <typename T>
std::optional<T> GetOptionalValue(const flutter::EncodableMap& map,
                                  const std::string& key) {
  const auto it = map.find(flutter::EncodableValue(key));
  if (it != map.end()) {
    const auto val = std::get_if<T>(&it->second);
    if (val) {
      return *val;
    }
  }
  return std::nullopt;
}

class WebviewWindowsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

  WebviewWindowsPlugin(flutter::TextureRegistrar* textures,
                       flutter::BinaryMessenger* messenger);

  virtual ~WebviewWindowsPlugin();

 private:
  std::unordered_map<int64_t, std::unique_ptr<WebviewBridge>> instances_;
  std::unique_ptr<WebviewHost> webview_host_;
  std::unique_ptr<GraphicsContext> graphics_context_;

  WNDCLASS window_class_ = {};
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

  auto plugin = std::make_unique<WebviewWindowsPlugin>(
      registrar->texture_registrar(), registrar->messenger());

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

WebviewWindowsPlugin::WebviewWindowsPlugin(flutter::TextureRegistrar* textures,
                                           flutter::BinaryMessenger* messenger)
    : textures_(textures), messenger_(messenger) {
  window_class_.lpszClassName = L"FlutterWebviewMessage";
  window_class_.lpfnWndProc = &DefWindowProc;
  RegisterClass(&window_class_);
}

WebviewWindowsPlugin::~WebviewWindowsPlugin() {
  instances_.clear();
  UnregisterClass(window_class_.lpszClassName, nullptr);
}

void WebviewWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  if (method_call.method_name().compare(kMethodInitializeEnvironment) == 0) {
    if (webview_host_) {
      return result->Error("already_initialized",
                           "The webview environment is already initialized");
    }

    const auto& map = std::get<flutter::EncodableMap>(*method_call.arguments());

    std::optional<std::string> browser_exe_path =
        GetOptionalValue<std::string>(map, "browserExePath");

    std::optional<std::string> user_data_path =
        GetOptionalValue<std::string>(map, "userDataPath");
    if (!user_data_path) {
      user_data_path = GetDefaultDataDirectory();
    }

    std::optional<std::string> additional_args =
        GetOptionalValue<std::string>(map, "additionalArguments");

    webview_host_ = std::move(
        WebviewHost::Create(user_data_path, browser_exe_path, additional_args));
    if (!webview_host_) {
      return result->Error(kErrorMessageEnvironmentCreationFailed);
    }

    return result->Success();
  }

  if (method_call.method_name().compare(kMethodInitialize) == 0) {
    return CreateWebviewInstance(std::move(result));
  }

  if (method_call.method_name().compare(kMethodDispose) == 0) {
    if (const auto texture_id = std::get_if<int64_t>(method_call.arguments())) {
      const auto it = instances_.find(*texture_id);
      if (it != instances_.end()) {
        instances_.erase(it);
        return result->Success();
      }
    }
    return result->Error("No such instance");
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
    webview_host_ = std::move(WebviewHost::Create(GetDefaultDataDirectory()));
    if (!webview_host_) {
      return result->Error(kErrorMessageEnvironmentCreationFailed);
    }
  }

  if (!graphics_context_) {
    graphics_context_ = std::make_unique<GraphicsContext>();
  }

  auto hwnd = CreateWindowEx(0, window_class_.lpszClassName, L"", 0, CW_DEFAULT,
                             CW_DEFAULT, 0, 0, HWND_MESSAGE, nullptr,
                             window_class_.hInstance, nullptr);

  std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> holder =
      std::move(result);
  webview_host_->CreateWebview(
      hwnd, true, true, [holder, this](std::unique_ptr<Webview> webview) {
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
