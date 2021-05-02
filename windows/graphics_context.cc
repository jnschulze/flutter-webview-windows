#include "graphics_context.h"

#include "util/d3dutil.h"
#include "util/direct3d11.interop.h"

GraphicsContext::GraphicsContext() {
  device_ = CreateD3DDevice();
  device_->GetImmediateContext(device_context_.put());
  device_winrt_ = CreateDirect3DDevice(device_.as<IDXGIDevice>().get());
}
