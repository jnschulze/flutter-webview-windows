#include "graphics_context.h"

#include "util/d3dutil.h"
#include "util/direct3d11.interop.h"

GraphicsContext::GraphicsContext() {

  d3dDevice_ = CreateD3DDevice();
  d3dDevice_->GetImmediateContext(device_context_.put());
  device_ = CreateDirect3DDevice(d3dDevice_.as<IDXGIDevice>().get());

}