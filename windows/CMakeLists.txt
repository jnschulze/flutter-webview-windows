cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "webview_windows")

set(WIL_VERSION "1.0.210204.1")
set(WEBVIEW_VERSION "1.0.865-prerelease")
set(WIN_SDK_VERSION_MIN 10.0.19041.0)

set(CMAKE_SYSTEM_VERSION ${WIN_SDK_VERSION_MIN} CACHE STRING INTERNAL FORCE)
set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION ${WIN_SDK_VERSION_MIN} CACHE STRING INTERNAL FORCE)

project(${PROJECT_NAME} LANGUAGES CXX)



# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "webview_windows_plugin")



add_subdirectory(third_party/fmt)

find_program(NUGET_EXE NAMES nuget)
if(NOT NUGET_EXE)
	message("NUGET.EXE not found.")
	message(FATAL_ERROR "Please install this executable, and run CMake again.")
endif()

exec_program(${NUGET_EXE}
    ARGS install "Microsoft.Web.WebView2" -Version ${WEBVIEW_VERSION} -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)
exec_program(${NUGET_EXE}
    ARGS install "Microsoft.Windows.ImplementationLibrary" -Version ${WIL_VERSION} -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages)

add_library(${PLUGIN_NAME} SHARED
  "webview_windows_plugin.cc"
  "webview.cc"
  "webview_host.cc"
  "webview_bridge.cc"
  "texture_bridge.cc"
  "graphics_context.cc"
)

# Enable AVX2 for pixel buffer conversions
if(MSVC)
  target_compile_options(${PLUGIN_NAME} PRIVATE "/arch:AVX2")
endif()


apply_standard_settings(${PLUGIN_NAME})
set_target_properties(${PLUGIN_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(${PLUGIN_NAME} PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION ${WIN_SDK_VERSION_MIN})

target_link_libraries(${PLUGIN_NAME} PRIVATE ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/Microsoft.Web.WebView2.targets)
target_link_libraries(${PLUGIN_NAME} PRIVATE ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.ImplementationLibrary/build/native/Microsoft.Windows.ImplementationLibrary.targets)

target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )

target_include_directories(${PLUGIN_NAME} PRIVATE "third_party/fmt/include")

target_link_libraries(${PLUGIN_NAME} PRIVATE fmt::fmt)
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

set(webview_windows_bundled_libraries
  PARENT_SCOPE
)
