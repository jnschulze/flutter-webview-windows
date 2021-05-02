#include "webview_bridge.h"

#include <flutter/event_stream_handler_functions.h>
#include <fmt/core.h>

#include <iostream>

namespace {
const std::string kErrorInvalidArgs = "invalidArguments";

const std::string kMethodLoadUrl = "loadUrl";
const std::string kMethodSetSize = "setSize";
const std::string kMethodSetCursorPos = "setCursorPos";
const std::string kMethodSetPointerButton = "setPointerButton";
const std::string kMethodSetScrollDelta = "setScrollDelta";

const std::string kEventType = "type";

static std::optional<std::pair<double, double>> GetPointFromArgs(
    const flutter::EncodableValue* args) {
  const flutter::EncodableList* list =
      std::get_if<flutter::EncodableList>(args);
  if (!list || list->size() != 2) {
    return std::nullopt;
  }
  auto x = std::get_if<double>(&(*list)[0]);
  auto y = std::get_if<double>(&(*list)[1]);
  if (!x || !y) {
    return std::nullopt;
  }
  return std::make_pair(*x, *y);
}

}  // namespace

WebviewBridge::WebviewBridge(flutter::BinaryMessenger* messenger,
                             flutter::TextureRegistrar* texture_registrar,
                             GraphicsContext* graphics_context,
                             std::unique_ptr<Webview> webview)
    : webview_(std::move(webview)) {
  texture_bridge_ = std::make_unique<TextureBridge>(graphics_context,
                                                    webview_->surface().get());

  flutter_texture_ =
      std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
          [this](size_t width,
                 size_t height) -> const FlutterDesktopPixelBuffer* {
            return texture_bridge_->CopyPixelBuffer(width, height);
          }));

  texture_id_ = texture_registrar->RegisterTexture(flutter_texture_.get());
  texture_bridge_->SetOnFrameAvailable([this, texture_registrar]() {
    texture_registrar->MarkTextureFrameAvailable(texture_id_);
  });
  // texture_bridge_->SetOnSurfaceSizeChanged([this](Size size) {
  //  webview_->SetSurfaceSize(size.width, size.height);
  //});

  auto method_channel_name = fmt::format("io.jns.webview.win/{}", texture_id_);
  method_channel_ =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          messenger, method_channel_name,
          &flutter::StandardMethodCodec::GetInstance());

  method_channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    HandleMethodCall(call, std::move(result));
  });

  auto event_channel_name =
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

void WebviewBridge::RegisterEventHandlers() {
  webview_->OnUrlChanged([this](const std::string& url) {
    auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("urlChanged")},
        {flutter::EncodableValue("url"), flutter::EncodableValue(url)},
    });
    event_sink_->Success(event);
  });

  webview_->OnLoadingStateChanged([this](WebviewLoadingState state) {
    auto event = flutter::EncodableValue(flutter::EncodableMap{
        {flutter::EncodableValue(kEventType),
         flutter::EncodableValue("loadingStateChanged")},
        {flutter::EncodableValue("value"), flutter::EncodableValue((int)state)},
    });
    event_sink_->Success(event);
  });

  webview_->OnSurfaceSizeChanged([this](size_t width, size_t height) {
    texture_bridge_->NotifySurfaceSizeChanged();
  });
}

void WebviewBridge::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  const auto& method_name = method_call.method_name();

  // setCursorPos: [double x, double y]
  if (method_name.compare(kMethodSetCursorPos) == 0) {
    auto point = GetPointFromArgs(method_call.arguments());
    if (point) {
      webview_->SetCursorPos(point->first, point->second);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setScrollDelta: [double dx, double dy]
  if (method_name.compare(kMethodSetScrollDelta) == 0) {
    auto delta = GetPointFromArgs(method_call.arguments());
    if (delta) {
      webview_->SetScrollDelta(delta->first, delta->second);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  // setPointerButton: {"button": int, "isDown": bool}
  if (method_name.compare(kMethodSetPointerButton) == 0) {
    const auto& map = std::get<flutter::EncodableMap>(*method_call.arguments());

    auto button = map.find(flutter::EncodableValue("button"));
    auto isDown = map.find(flutter::EncodableValue("isDown"));
    if (button != map.end() && isDown != map.end()) {
      auto buttonValue = std::get_if<int>(&button->second);
      auto isDownValue = std::get_if<bool>(&isDown->second);
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
    if (auto url = std::get_if<std::string>(method_call.arguments())) {
      webview_->LoadUrl(*url);
      return result->Success();
    }
    return result->Error(kErrorInvalidArgs);
  }

  result->NotImplemented();
}