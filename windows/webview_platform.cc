#include "webview_platform.h"

#include <DispatcherQueue.h>
#include <shlobj.h>
#include <winrt/Windows.Graphics.Capture.h>

#include <filesystem>
#include <iostream>

WebviewPlatform::WebviewPlatform()
    : rohelper_(std::make_unique<rx::RoHelper>(RO_INIT_SINGLETHREADED)) {
  if (rohelper_->WinRtAvailable()) {
    DispatcherQueueOptions options{sizeof(DispatcherQueueOptions),
                                   DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};

    if (!SUCCEEDED(rohelper_->CreateDispatcherQueueController(
            options, dispatcher_queue_controller_.put()))) {
      std::cerr << "Creating DispatcherQueueController failed." << std::endl;
      return;
    }

    if (!winrt::Windows::Graphics::Capture::GraphicsCaptureSession::
            IsSupported()) {
      std::cerr << "Windows::Graphics::Capture::GraphicsCaptureSession is not "
                   "supported."
                << std::endl;
      return;
    }

    valid_ = true;
  }
}

std::optional<std::string> WebviewPlatform::GetDefaultDataDirectory() {
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
