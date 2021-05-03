#include "webview.h"

#include <atlstr.h>
#include <fmt/core.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.UI.Core.h>

#include <iostream>
#include <optional>

#include "webview_host.h"

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

Webview::Webview(
    wil::com_ptr<ICoreWebView2CompositionController> composition_controller,
    WebviewHost* host, std::optional<HWND> hwnd)
    : composition_controller_(std::move(composition_controller)), host_(host) {
  webview_controller_ =
      composition_controller_.query<ICoreWebView2Controller3>();
  webview_controller_->get_CoreWebView2(webview_.put());

  webview_controller_->put_BoundsMode(COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);
  webview_controller_->put_ShouldDetectMonitorScaleChanges(FALSE);
  webview_controller_->put_RasterizationScale(1.0);

  wil::com_ptr<ICoreWebView2Settings> settings;
  if (webview_->get_Settings(settings.put()) == S_OK) {
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);
  }

  RegisterEventHandlers();

  auto compositor = host->compositor();
  auto root = compositor.CreateContainerVisual();

  // initial size. doesn't matter as we resize the surface anyway.
  root.Size({1280, 720});
  root.IsVisible(true);
  surface_ = root.as<winrt::Windows::UI::Composition::Visual>();

  // Create on-screen window for debugging purposes
  if (hwnd) {
    window_target_ = CreateDesktopWindowTarget(compositor, hwnd.value());
    window_target_.Root(root);
  }

  auto webview_visual = compositor.CreateSpriteVisual();
  webview_visual.RelativeSizeAdjustment({1.0f, 1.0f});

  root.Children().InsertAtTop(webview_visual);

  composition_controller_->put_RootVisualTarget(
      webview_visual.as<IUnknown>().get());

  webview_controller_->put_IsVisible(true);
}

void Webview::RegisterEventHandlers() {
  webview_->add_ContentLoading(
      Callback<ICoreWebView2ContentLoadingEventHandler>(
          [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
            if (loading_state_changed_callback_) {
              loading_state_changed_callback_(WebviewLoadingState::Loading);
            }

            return S_OK;
          })
          .Get(),
      &content_loading_token_);

  webview_->add_NavigationCompleted(
      Callback<ICoreWebView2NavigationCompletedEventHandler>(
          [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
            if (loading_state_changed_callback_) {
              loading_state_changed_callback_(
                  WebviewLoadingState::NavigationCompleted);
            }

            return S_OK;
          })
          .Get(),
      &navigation_completed_token_);

  webview_->add_SourceChanged(
      Callback<ICoreWebView2SourceChangedEventHandler>(
          [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
            LPWSTR wurl;
            if (url_changed_callback_ && webview_->get_Source(&wurl) == S_OK) {
              std::string url = CW2A(wurl);
              url_changed_callback_(url);
            }

            return S_OK;
          })
          .Get(),
      &source_changed_token_);

  webview_->add_DocumentTitleChanged(
      Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
          [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
            LPWSTR wtitle;
            if (document_title_changed_callback_ &&
                webview_->get_DocumentTitle(&wtitle) == S_OK) {
              std::string title = CW2A(wtitle);
              document_title_changed_callback_(title);
            }

            return S_OK;
          })
          .Get(),
      &document_title_changed_token_);

  composition_controller_->add_CursorChanged(
      Callback<ICoreWebView2CursorChangedEventHandler>(
          [this](ICoreWebView2CompositionController* sender,
                 IUnknown* args) -> HRESULT {
            HCURSOR cursor;
            if (cursor_changed_callback_ &&
                sender->get_Cursor(&cursor) == S_OK) {
              cursor_changed_callback_(cursor);
            }
            return S_OK;
          })
          .Get(),
      &cursor_changed_token_);
}

void Webview::SetSurfaceSize(size_t width, size_t height) {
  auto surface = surface_.get();

  if (surface) {
    surface.Size({(float)width, (float)height});

    RECT bounds;
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = static_cast<LONG>(width);
    bounds.bottom = static_cast<LONG>(height);

    if (webview_controller_->put_Bounds(bounds) != S_OK) {
      std::cerr << "Setting webview bounds failed." << std::endl;
    }

    if (surface_size_changed_callback_) {
      surface_size_changed_callback_(width, height);
    }
  }
}

bool Webview::ClearCookies() {
  return webview_->CallDevToolsProtocolMethod(L"Network.clearBrowserCookies",
                                              L"{}", nullptr) == S_OK;
}

void Webview::SetCursorPos(double x, double y) {
  POINT p;
  p.x = static_cast<LONG>(x);
  p.y = static_cast<LONG>(y);

  last_cursor_pos_ = p;

  // https://docs.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2?view=webview2-1.0.774.44
  composition_controller_->SendMouseInput(
      COREWEBVIEW2_MOUSE_EVENT_KIND::COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE,
      COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
          COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE,
      0, p);
}

void Webview::SetPointerButtonState(WebviewPointerButton button, bool isDown) {
  COREWEBVIEW2_MOUSE_EVENT_KIND kind;
  switch (button) {
    case WebviewPointerButton::Primary:
      kind = isDown ? COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN
                    : COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP;
      break;
    case WebviewPointerButton::Secondary:
      kind = isDown ? COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN
                    : COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP;
      break;
    case WebviewPointerButton::Tertiary:
      kind = isDown ? COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN
                    : COREWEBVIEW2_MOUSE_EVENT_KIND::
                          COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP;
      break;
    default:
      kind = static_cast<COREWEBVIEW2_MOUSE_EVENT_KIND>(0);
  }

  composition_controller_->SendMouseInput(
      kind,
      COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
          COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE,
      0, last_cursor_pos_);
}

void Webview::SetScrollDelta(double delta_x, double delta_y) {
  if (delta_x != 0.0) {
    auto delta = static_cast<short>(delta_x * WHEEL_DELTA);
    composition_controller_->SendMouseInput(
        COREWEBVIEW2_MOUSE_EVENT_KIND::
            COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL,
        COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
            COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE,
        delta, last_cursor_pos_);
  }
  if (delta_y != 0.0) {
    auto delta = static_cast<short>(delta_y * WHEEL_DELTA);
    composition_controller_->SendMouseInput(
        COREWEBVIEW2_MOUSE_EVENT_KIND::COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL,
        COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS::
            COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE,
        delta, last_cursor_pos_);
  }
}

void Webview::LoadUrl(const std::string& url) {
  std::wstring wurl(url.begin(), url.end());
  webview_->Navigate(wurl.c_str());
}

void Webview::LoadStringContent(const std::string& content) {
  std::wstring wcontent(content.begin(), content.end());
  webview_->NavigateToString(wcontent.c_str());
}

void Webview::Reload() { webview_->Reload(); }
