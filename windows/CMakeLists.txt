cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "webview_windows")

set(WIL_VERSION "1.0.220201.1")
set(WEBVIEW_VERSION "1.0.1210.39")

project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "webview_windows_plugin")

add_subdirectory(third_party/fmt)

add_library(${PLUGIN_NAME} SHARED
  "webview_windows_plugin.cc"
  "webview_platform.cc"
  "webview.cc"
  "webview_host.cc"
  "webview_bridge.cc"
  "texture_bridge.cc"
  "graphics_context.cc"
  "util/direct3d11.interop.cc"
  "util/rohelper.cc"
  "util/string_converter.cc"
)

if(FLUTTER_WEBVIEW_WINDOWS_USE_D3D_TEXTURE)
  message(STATUS "Building with D3D texture support.")
  target_compile_definitions("${PLUGIN_NAME}" PRIVATE
    HAVE_FLUTTER_D3D_TEXTURE
  )
  target_sources("${PLUGIN_NAME}" PRIVATE
    "texture_bridge_gpu.cc"
  )
else()
  message(STATUS "Building with fallback PixelBuffer texture.")
  target_sources("${PLUGIN_NAME}" PRIVATE
    "texture_bridge_fallback.cc"
    "util/cpuid/cpuinfo.cc"
  )
  # Enable AVX2 for pixel buffer conversions
  if(MSVC)
    target_compile_options(${PLUGIN_NAME} PRIVATE "/arch:AVX2")
  endif()
endif()

apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

target_include_directories(${PLUGIN_NAME} PRIVATE "third_party/fmt/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE fmt::fmt)
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

add_library(Webview2 SHARED IMPORTED GLOBAL)
SET_PROPERTY(TARGET Webview2 PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/libs/x64/Webview2Loader.dll)
SET_PROPERTY(TARGET Webview2 PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/libs/x64/Webview2Loader.dll.lib)
target_link_libraries(${PLUGIN_NAME} PRIVATE Webview2 uxtheme)

set(webview_windows_bundled_libraries
  "$<TARGET_FILE:Webview2>"
  PARENT_SCOPE
)
