#include "webview_bridge.h"

#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_result_functions.h>
#include <fmt/core.h>

#include <iostream>

namespace {
constexpr auto kErrorInvalidArgs = "invalidArguments";

constexpr auto kMethodLoadUrl = "loadUrl";
constexpr auto kMethodLoadStringContent = "loadStringContent";
constexpr auto kMethodReload = "reload";
constexpr auto kMethodStop = "stop";
constexpr auto kMethodGoBack = "goBack";
constexpr auto kMethodGoForward = "goForward";
constexpr auto kMethodExecuteScript = "executeScript";
constexpr auto kMethodPostWebMessage = "postWebMessage";
constexpr auto kMethodSetSize = "setSize";
constexpr auto kMethodSetCursorPos = "setCursorPos";
constexpr auto kMethodSetPointerButton = "setPointerButton";
constexpr auto kMethodSetScrollDelta = "setScrollDelta";
constexpr auto kMethodSetUserAgent = "setUserAgent";
constexpr auto kMethodSetBackgroundColor = "setBackgroundColor";

constexpr auto kEventType = "type";
constexpr auto kEventValue = "value";

constexpr auto kErrorNotSupported = "not_supported";
constexpr auto kScriptFailed = "script_failed";
constexpr auto kMethodFailed = "method_failed";

static const std::optional<std::pair<double, double>> GetPointFromArgs(
    const flutter::EncodableValue* args) {
  const flutter::EncodableList* list =
      std::get_if<flutter::EncodableList>(args);
  if (!list || list->size() != 2) {
    return std::nullopt;
  }
  const auto x = std::get_if<double>(&(*list)[0]);
  const auto y = std::get_if<double>(&(*list)[1]);
  if (!x || !y) {
    return std::nullopt;
  }
  return std::make_pair(*x, *y);
}

static const std::string& GetCursorName(const HCURSOR cursor) {
  // The cursor names correspond to the Flutter Engine names:
  // in shell/platform/windows/flutter_window_win32.cc
  static const std::string kDefaultCursorName = "basic";
  static const std::pair<std::string, const wchar_t*> mappings[] = {
      {"allScroll", IDC_SIZEALL},
      {kDefaultCursorName, IDC_ARROW},
      {"click", IDC_HAND},
      {"forbidden", IDC_NO},
      {"help", IDC_HELP},
      {"move", IDC_SIZEALL},
      {"none", nullptr},
      {"noDrop", IDC_NO},
      {"precise", IDC_CROSS},
      {"progress", IDC_APPSTARTING},
      {"text", IDC_IBEAM},
      {"resizeColumn", IDC_SIZEWE},
      {"resizeDown", IDC_SIZENS},
      {"resizeDownLeft", IDC_SIZENESW},
      {"resizeDownRight", IDC_SIZENWSE},
      {"resizeLeft", IDC_SIZEWE},
      {"resizeLeftRight", IDC_SIZEWE},
      {"resizeRight", IDC_SIZEWE},
      {"resizeRow", IDC_SIZENS},
      {"resizeUp", IDC_SIZENS},
      {"resizeUpDown", IDC_SIZENS},
      {"resizeUpLeft", IDC_SIZENWSE},
      {"resizeUpRight", IDC_SIZENESW},
      {"resizeUpLeftDownRight", IDC_SIZENWSE},
      {"resizeUpRightDownLeft", IDC_SIZENESW},
      {"wait", IDC_WAIT},
  };

  static std::map<HCURSOR, std::string> cursors;
  static bool initialized = false;

  if (!initialized) {
    initialized = true;
    for (const auto& pair : mappings) {
      HCURSOR cursor_handle = LoadCursor(nullptr, pair.second);
      if (cursor_handle) {
        cursors[cursor_handle] = pair.first;
      }
    }
  }

  const auto it = cursors.find(cursor);
  if (it != cursors.end()) {
    return it->second;
  }
  return kDefaultCursorName;
}

}  // namespace

WebviewBridge::WebviewBridge(flutter::BinaryMessenger* messenger,
                             flutter::TextureRegistrar* texture_registrar,
                             GraphicsContext* graphics_context,
                             std::unique_ptr<Webview> webview)
    : webview_(std::move(webview)), texture_registrar_(texture_registrar) {
  texture_bridge_ = std::make_unique<TextureBridge>(graphics_context,
                                                    webview_->surface().get());

  flutter_texture_ =
      std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
          [this](size_t width,
                 size_t height) -> const FlutterDesktopPixelBuffer* {
            return texture_bridge_->CopyPixelBuffer(width, height);
          }));

  texture_id_ = texture_registrar->RegisterTexture(flutter_texture_.get());
  texture_bridge_->SetOnFrameAvailable(
      [this]() { texture_registrar_->MarkTextureFrameAvailable(texture_id_); });
  // texture_bridge_->SetOnSurfaceSizeChanged([this](Size size) {
  //  webview_->SetSurfaceSize(size.width, size.height);
  //});

  const auto method_channel_name =
      fmt::format("io.jns.webview.win/{}", texture_id_);
  method_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          messenger, method_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  method_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    HandleMethodCall(call, std::move(result));
  });

  const auto event_channel_name =
      fmt::format("io.jns.webview.win/{}/events", texture_id_);
  event_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, event_channel_name,
          &flutter::StandardMethodCodec::GetInstance());

  auto handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](const flutter::EncodableValue* arguments,
             std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&&
                 events) {
        event_sink_ = std::move(events);
        RegisterEventHandlers();
        return nullptr;
      },
      [](const flutter::EncodableValue* arguments) { return nullptr; });

  event_channel_->SetStreamHandler(std::move(handler));
}

WebviewBridge::~WebviewBridge() {
  texture_registrar_->UnregisterTexture(texture_id_);
}

void WebviewBridge::RegisterEventHandlers() {
  webview_->OnUrlChanged([this](const std::string& url) {
    const auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("urlChanged")},
        {flutter::EncodableValue(kEventValue), flutter::EncodableValue(url)},
    });
    event_sink_->Success(event);
  });

  webview_->OnLoadingStateChanged([this](WebviewLoadingState state) {
    const auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("loadingStateChanged")},
        {flutter::EncodableValue(kEventValue),
         flutter::EncodableValue(static_cast<int>(state))},
    });
    event_sink_->Success(event);
  });

  webview_->OnHistoryChanged([this](WebviewHistoryChanged historyChanged) {
    const auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("historyChanged")},
        {flutter::EncodableValue(kEventValue),
         flutter::EncodableValue(
          flutter::EncodableMap{
           {flutter::EncodableValue("canGoBack"),
           flutter::EncodableValue(static_cast<bool>(historyChanged.can_go_back))},
           {flutter::EncodableValue("canGoForward"),
           flutter::EncodableValue(static_cast<bool>(historyChanged.can_go_forward))},
          }
         )
         },
    });
    event_sink_->Success(event);
  });

  webview_->OnDocumentTitleChanged([this](const std::string& title) {
    const auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("titleChanged")},
        {flutter::EncodableValue(kEventValue), flutter::EncodableValue(title)},
    });
    event_sink_->Success(event);
  });

  webview_->OnSurfaceSizeChanged([this](size_t width, size_t height) {
    texture_bridge_->NotifySurfaceSizeChanged();
  });

  webview_->OnCursorChanged([this](const HCURSOR cursor) {
    const auto& name = GetCursorName(cursor);
    const auto event = flutter::EncodableValue(
        flutter::EncodableMap{{flutter::EncodableValue(kEventType),
                               flutter::EncodableValue("cursorChanged")},
                              {flutter::EncodableValue(kEventValue), name}});
    event_sink_->Success(event);
  });

  webview_->OnWebMessageReceived([this](const std::string& message) {
    const auto event = flutter::EncodableValue(
        flutter::EncodableMap{{flutter::EncodableValue(kEventType),
                               flutter::EncodableValue("webMessageReceived")},
                              {flutter::EncodableValue(kEventValue), message}});
    event_sink_->Success(event);
  });

  webview_->OnPermissionRequested(
      [this](const std::string& url, WebviewPermissionKind kind,
             bool is_user_initiated,
             Webview::WebviewPermissionRequestedCompleter completer) {
        OnPermissionRequested(url, kind, is_user_initiated, completer);
      });
}

void WebviewBridge::OnPermissionRequested(
    const std::string& url, WebviewPermissionKind permissionKind,
    bool isUserInitiated,
    Webview::WebviewPermissionRequestedCompleter completer) {
  auto args = std::make_unique<flutter::EncodableValue>(flutter::EncodableMap{
      {"url", url},
      {"isUserInitiated", isUserInitiated},
      {"permissionKind", static_cast<int>(permissionKind)}});

  method_channel_->InvokeMethod(
      "permissionRequested", std::move(args),
      std::make_unique<flutter::MethodResultFunctions<flutter::EncodableValue>>(
          [completer](const flutter::EncodableValue* result) {
            auto allow = std::get_if<bool>(result);
            if (allow != nullptr) {
              return completer(*allow ? WebviewPermissionState::Allow
                                      : WebviewPermissionState::Deny);
            }
            completer(WebviewPermissionState::Default);
          },
          [completer](const std::string& error_code,
                      const std::string& error_message,
                      const flutter::EncodableValue* error_details) {
            completer(WebviewPermissionState::Default);
          },
          [completer]() { completer(WebviewPermissionState::Default); }));
}

void WebviewBridge::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const auto& method_name = method_call.method_name();

  // setCursorPos: [double x, double y]
  if (method_name.compare(kMethodSetCursorPos) == 0) {
    const auto point = GetPointFromArgs(method_call.arguments());
    if (point) {
      webview_->SetCursorPos(point->first, point->second);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setScrollDelta: [double dx, double dy]
  if (method_name.compare(kMethodSetScrollDelta) == 0) {
    const auto delta = GetPointFromArgs(method_call.arguments());
    if (delta) {
      webview_->SetScrollDelta(delta->first, delta->second);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setPointerButton: {"button": int, "isDown": bool}
  if (method_name.compare(kMethodSetPointerButton) == 0) {
    const auto& map = std::get<flutter::EncodableMap>(*method_call.arguments());

    const auto button = map.find(flutter::EncodableValue("button"));
    const auto isDown = map.find(flutter::EncodableValue("isDown"));
    if (button != map.end() && isDown != map.end()) {
      const auto buttonValue = std::get_if<int32_t>(&button->second);
      const auto isDownValue = std::get_if<bool>(&isDown->second);
      if (buttonValue && isDownValue) {
        webview_->SetPointerButtonState(
            static_cast<WebviewPointerButton>(*buttonValue), *isDownValue);
        return result->Success();
      }
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setSize: [double width, double height]
  if (method_name.compare(kMethodSetSize) == 0) {
    auto size = GetPointFromArgs(method_call.arguments());
    if (size) {
      webview_->SetSurfaceSize(static_cast<size_t>(size->first),
                               static_cast<size_t>(size->second));

      texture_bridge_->Start();
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // loadUrl: string
  if (method_name.compare(kMethodLoadUrl) == 0) {
    if (const auto url = std::get_if<std::string>(method_call.arguments())) {
      webview_->LoadUrl(*url);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // loadStringContent: string
  if (method_name.compare(kMethodLoadStringContent) == 0) {
    if (const auto content =
            std::get_if<std::string>(method_call.arguments())) {
      webview_->LoadStringContent(*content);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // reload
  if (method_name.compare(kMethodReload) == 0) {
    if (webview_->Reload()) {
      return result->Success();
    }
    return result->Error(kMethodFailed);
  }

  // stop
  if (method_name.compare(kMethodStop) == 0) {
    if (webview_->Stop()) {
      return result->Success();
    }
    return result->Error(kMethodFailed);
  }

  // goBack
  if (method_name.compare(kMethodGoBack) == 0) {
    if (webview_->GoBack()) {
      return result->Success();
    }
    return result->Error(kMethodFailed);
  }

  // goForward
  if (method_name.compare(kMethodGoForward) == 0) {
    if (webview_->GoForward()) {
      return result->Success();
    }
    return result->Error(kMethodFailed);
  }

  // executeScript: string
  if (method_name.compare(kMethodExecuteScript) == 0) {
    if (const auto script = std::get_if<std::string>(method_call.arguments())) {
      std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>
          shared_result = std::move(result);

      webview_->ExecuteScript(*script, [shared_result](bool success) {
        if (success) {
          shared_result->Success();
        } else {
          shared_result->Error(kScriptFailed, "Executing script failed.");
        }
      });
      return;
    }
    return result->Error(kErrorInvalidArgs);
  }

  // postWebMessage: string
  if (method_name.compare(kMethodPostWebMessage) == 0) {
    if (const auto message =
            std::get_if<std::string>(method_call.arguments())) {
      if (webview_->PostWebMessage(*message)) {
        return result->Success();
      }
      return result->Error(kErrorNotSupported, "Posting the message failed.");
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setUserAgent: string
  if (method_name.compare(kMethodSetUserAgent) == 0) {
    if (const auto user_agent =
            std::get_if<std::string>(method_call.arguments())) {
      if (webview_->SetUserAgent(*user_agent)) {
        return result->Success();
      }
      return result->Error(kErrorNotSupported,
                           "Setting the user agent failed.");
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setBackgroundColor: int
  if (method_name.compare(kMethodSetBackgroundColor) == 0) {
    if (const auto color = std::get_if<int32_t>(method_call.arguments())) {
      if (webview_->SetBackgroundColor(*color)) {
        return result->Success();
      }
      return result->Error(kErrorNotSupported,
                           "Setting the background color failed.");
    }
    return result->Error(kErrorInvalidArgs);
  }

  result->NotImplemented();
}
