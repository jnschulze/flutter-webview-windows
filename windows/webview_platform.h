#pragma once

#include <winrt/base.h>

#include <memory>
#include <optional>
#include <string>

#include "util/rohelper.h"

class WebviewPlatform {
 public:
  WebviewPlatform();
  bool IsSupported() { return valid_; }
  std::optional<std::string> GetDefaultDataDirectory();

 private:
  std::unique_ptr<rx::RoHelper> rohelper_;
  winrt::com_ptr<ABI::Windows::System::IDispatcherQueueController>
      dispatcher_queue_controller_;
  bool valid_ = false;
};
