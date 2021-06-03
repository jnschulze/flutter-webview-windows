#pragma once

#include <WebView2.h>
#include <wil/com.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <wrl.h>

#include <functional>

class WebviewHost;

enum class WebviewLoadingState { None, Loading, NavigationCompleted };

enum class WebviewPointerButton { None, Primary, Secondary, Tertiary };

struct VirtualKeyState {
 public:
  inline void set_isLeftButtonDown(bool is_down) {
    set(COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
            COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON,
        is_down);
  }

  inline void set_isRightButtonDown(bool is_down) {
    set(COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
            COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON,
        is_down);
  }

  inline void set_isMiddleButtonDown(bool is_down) {
    set(COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
            COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON,
        is_down);
  }

  inline COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS state() const { return state_; }

 private:
  COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS state_ =
      COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
          COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;

  inline void set(COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS key, bool flag) {
    if (flag) {
      state_ |= key;
    } else {
      state_ &= ~key;
    }
  }
};

class Webview {
 public:
  friend class WebviewHost;

  typedef std::function<void(const std::string&)> UrlChangedCallback;
  typedef std::function<void(WebviewLoadingState)> LoadingStateChangedCallback;
  typedef std::function<void(const std::string&)> DocumentTitleChangedCallback;
  typedef std::function<void(size_t width, size_t height)>
      SurfaceSizeChangedCallback;
  typedef std::function<void(const HCURSOR)> CursorChangedCallback;
  typedef std::function<void(bool)> FocusChangedCallback;
  typedef std::function<void(bool)> ScriptExecutedCallback;
  typedef std::function<void(const std::string&)> WebMessageReceivedCallback;

  ~Webview();

  winrt::agile_ref<winrt::Windows::UI::Composition::Visual> const surface() {
    return surface_;
  }

  void SetSurfaceSize(size_t width, size_t height);
  void SetCursorPos(double x, double y);
  void SetPointerButtonState(WebviewPointerButton button, bool isDown);
  void SetScrollDelta(double delta_x, double delta_y);
  void LoadUrl(const std::string& url);
  void LoadStringContent(const std::string& content);
  void Reload();
  void ExecuteScript(const std::string& script, ScriptExecutedCallback callback);
  bool PostWebMessage(const std::string& json);
  bool ClearCookies();
  bool SetUserAgent(const std::string& user_agent);
  bool SetBackgroundColor(int32_t color);

  void OnUrlChanged(UrlChangedCallback callback) {
    url_changed_callback_ = std::move(callback);
  }

  void OnLoadingStateChanged(LoadingStateChangedCallback callback) {
    loading_state_changed_callback_ = std::move(callback);
  }

  void OnSurfaceSizeChanged(SurfaceSizeChangedCallback callback) {
    surface_size_changed_callback_ = std::move(callback);
  }

  void OnDocumentTitleChanged(DocumentTitleChangedCallback callback) {
    document_title_changed_callback_ = std::move(callback);
  }

  void OnCursorChanged(CursorChangedCallback callback) {
    cursor_changed_callback_ = std::move(callback);
  }

  void OnFocusChanged(FocusChangedCallback callback) {
    focus_changed_callback_ = std::move(callback);
  }

  void OnWebMessageReceived(WebMessageReceivedCallback callback) {
    web_message_received_callback_ = std::move(callback);
  }

 private:
  HWND hwnd_;
  bool owns_window_;
  wil::com_ptr<ICoreWebView2CompositionController> composition_controller_;
  wil::com_ptr<ICoreWebView2Controller3> webview_controller_;
  wil::com_ptr<ICoreWebView2> webview_;
  wil::com_ptr<ICoreWebView2Settings2> settings2_;
  POINT last_cursor_pos_ = {0, 0};
  VirtualKeyState virtual_keys_;

  winrt::agile_ref<winrt::Windows::UI::Composition::Visual> surface_{nullptr};

  winrt::agile_ref<winrt::Windows::UI::Core::CoreDispatcher> dispatcher_{
      nullptr};

  winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget window_target_{
      nullptr};

  WebviewHost* host_;

  EventRegistrationToken source_changed_token_ = {};
  EventRegistrationToken content_loading_token_ = {};
  EventRegistrationToken navigation_completed_token_ = {};
  EventRegistrationToken document_title_changed_token_ = {};
  EventRegistrationToken cursor_changed_token_ = {};
  EventRegistrationToken got_focus_token_ = {};
  EventRegistrationToken lost_focus_token_ = {};
  EventRegistrationToken web_message_received_token_ = {};

  UrlChangedCallback url_changed_callback_;
  LoadingStateChangedCallback loading_state_changed_callback_;
  DocumentTitleChangedCallback document_title_changed_callback_;
  SurfaceSizeChangedCallback surface_size_changed_callback_;
  CursorChangedCallback cursor_changed_callback_;
  FocusChangedCallback focus_changed_callback_;
  WebMessageReceivedCallback web_message_received_callback_;

  Webview(
      wil::com_ptr<ICoreWebView2CompositionController> composition_controller,
      WebviewHost* host, HWND hwnd, bool owns_window, bool offscreen_only);

  void RegisterEventHandlers();
  void SendScroll(double offset, bool horizontal);
};
