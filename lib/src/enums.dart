// Order must match WebviewLoadingState (see webview.h)
enum LoadingState { none, loading, navigationCompleted }

// Order must match WebviewPointerButton (see webview.h)
enum PointerButton { none, primary, secondary, tertiary }

// Order must match WebviewPermissionKind (see webview.h)
enum WebviewPermissionKind {
  unknown,
  microphone,
  camera,
  geoLocation,
  notifications,
  otherSensors,
  clipboardRead
}

enum WebviewPermissionDecision { none, allow, deny }
