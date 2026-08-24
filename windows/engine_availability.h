#pragma once

#include <flutter_messenger.h>

#include <atomic>

namespace webview_windows {

// The raw messenger, captured once at plugin registration and deliberately
// kept (AddRef, never Release) for the life of the process. Teardown paths
// use it to ask whether the engine is still alive before calling any
// messenger- or registrar-backed API.
//
// Why this exists: FlutterWindowsEngine's destructor clears the messenger's
// engine pointer BEFORE FlutterWindowsEngine::Stop() runs the plugin
// destruction callbacks (upstream flutter/flutter#118611), so a plugin
// destructor that unregisters a channel handler or texture dereferences a
// null engine - an access violation on every app exit that still has a live
// webview. FlutterDesktopMessengerIsAvailable is the embedder API that
// exists precisely for this check.
inline std::atomic<FlutterDesktopMessengerRef>& MessengerSlot() {
  static std::atomic<FlutterDesktopMessengerRef> messenger{nullptr};
  return messenger;
}

inline void CaptureMessengerForAvailabilityChecks(
    FlutterDesktopMessengerRef messenger) {
  if (!messenger) {
    return;
  }
  // Single-engine assumption: a second registration (multi-engine app)
  // overwrites the slot with the newest messenger. This plugin is already
  // single-instance per engine, and the check below only ever needs SOME
  // live messenger from the current engine.
  FlutterDesktopMessengerAddRef(messenger);
  MessengerSlot().store(messenger, std::memory_order_release);
}

// True from plugin construction until ~WebviewWindowsPlugin begins. The
// plugin is destroyed exactly once, by the registrar destruction handler
// that runs during engine teardown - so a WebviewBridge destroyed while
// this is false is being torn down WITH the engine and must not call back
// into it. A bridge destroyed while it is true is a mid-session dispose
// and unregisters exactly as the plugin always has. All of this happens on
// the platform thread; the atomic is only for the creation-completion
// callback's benefit.
inline std::atomic<bool>& PluginAliveFlag() {
  static std::atomic<bool> alive{false};
  return alive;
}

inline void SetPluginAlive(bool value) {
  PluginAliveFlag().store(value, std::memory_order_release);
}

inline bool PluginAlive() {
  return PluginAliveFlag().load(std::memory_order_acquire);
}

// True when it is safe to call into the engine (channels, texture
// registrar). Returns true when no messenger was ever captured, preserving
// the plugin's original behavior for hosts that don't go through the
// C entry point.
inline bool EngineAvailable() {
  FlutterDesktopMessengerRef messenger =
      MessengerSlot().load(std::memory_order_acquire);
  if (!messenger) {
    return true;
  }
  FlutterDesktopMessengerLock(messenger);
  const bool available = FlutterDesktopMessengerIsAvailable(messenger);
  FlutterDesktopMessengerUnlock(messenger);
  return available;
}

// Runs |fn| while the messenger lock guarantees the engine cannot be torn
// down underneath it, or not at all if the engine is already gone. This is
// the form for engine calls made OFF the platform thread (e.g. the texture
// frame callback from the capture thread): the header contract is that
// availability does not change while the lock is held, so the engine's
// destructor blocks on SetEngine(nullptr) until |fn| returns. Runs |fn|
// unguarded when no messenger was captured, preserving original behavior.
template <typename F>
inline void IfEngineAvailableLocked(F&& fn) {
  FlutterDesktopMessengerRef messenger =
      MessengerSlot().load(std::memory_order_acquire);
  if (!messenger) {
    fn();
    return;
  }
  FlutterDesktopMessengerLock(messenger);
  if (FlutterDesktopMessengerIsAvailable(messenger)) {
    fn();
  }
  FlutterDesktopMessengerUnlock(messenger);
}

}  // namespace webview_windows
