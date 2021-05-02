#pragma once

#include <DispatcherQueue.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

inline winrt::Windows::System::DispatcherQueueController
CreateDispatcherQueueController() {
  DispatcherQueueOptions options{sizeof(DispatcherQueueOptions),
                                 DQTYPE_THREAD_CURRENT, DQTAT_COM_STA};

  winrt::Windows::System::DispatcherQueueController controller{nullptr};
  winrt::check_hresult(CreateDispatcherQueueController(
      options,
      reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(
          winrt::put_abi(controller))));
  return controller;
}
