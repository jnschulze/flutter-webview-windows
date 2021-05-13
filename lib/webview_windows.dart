import 'dart:async';
import 'dart:convert';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

// Order must match WebviewLoadingState (see webview.h)
enum LoadingState { None, Loading, NavigationCompleted }

// Order must match WebviewPointerButton (see webview.h)
enum PointerButton { None, Primary, Secondary, Tertiary }

/// Attempts to translate a button constant such as [kPrimaryMouseButton]
/// to a [PointerButton]
PointerButton _getButton(int value) {
  switch (value) {
    case kPrimaryMouseButton:
      return PointerButton.Primary;
    case kSecondaryMouseButton:
      return PointerButton.Secondary;
    case kTertiaryButton:
      return PointerButton.Tertiary;
    default:
      return PointerButton.None;
  }
}

const Map<String, SystemMouseCursor> _cursors = {
  'none': SystemMouseCursors.none,
  'basic': SystemMouseCursors.basic,
  'click': SystemMouseCursors.click,
  'forbidden': SystemMouseCursors.forbidden,
  'wait': SystemMouseCursors.wait,
  'progress': SystemMouseCursors.progress,
  'contextMenu': SystemMouseCursors.contextMenu,
  'help': SystemMouseCursors.help,
  'text': SystemMouseCursors.text,
  'verticalText': SystemMouseCursors.verticalText,
  'cell': SystemMouseCursors.cell,
  'precise': SystemMouseCursors.precise,
  'move': SystemMouseCursors.move,
  'grab': SystemMouseCursors.grab,
  'grabbing': SystemMouseCursors.grabbing,
  'noDrop': SystemMouseCursors.noDrop,
  'alias': SystemMouseCursors.alias,
  'copy': SystemMouseCursors.copy,
  'disappearing': SystemMouseCursors.disappearing,
  'allScroll': SystemMouseCursors.allScroll,
  'resizeLeftRight': SystemMouseCursors.resizeLeftRight,
  'resizeUpDown': SystemMouseCursors.resizeUpDown,
  'resizeUpLeftDownRight': SystemMouseCursors.resizeUpLeftDownRight,
  'resizeUpRightDownLeft': SystemMouseCursors.resizeUpRightDownLeft,
  'resizeUp': SystemMouseCursors.resizeUp,
  'resizeDown': SystemMouseCursors.resizeDown,
  'resizeLeft': SystemMouseCursors.resizeLeft,
  'resizeRight': SystemMouseCursors.resizeRight,
  'resizeUpLeft': SystemMouseCursors.resizeUpLeft,
  'resizeUpRight': SystemMouseCursors.resizeUpRight,
  'resizeDownLeft': SystemMouseCursors.resizeDownLeft,
  'resizeDownRight': SystemMouseCursors.resizeDownRight,
  'resizeColumn': SystemMouseCursors.resizeColumn,
  'resizeRow': SystemMouseCursors.resizeRow,
  'zoomIn': SystemMouseCursors.zoomIn,
  'zoomOut': SystemMouseCursors.zoomOut,
};

SystemMouseCursor _getCursorByName(String name) =>
    _cursors[name] ?? SystemMouseCursors.basic;

const String _pluginChannelPrefix = 'io.jns.webview.win';
final MethodChannel _pluginChannel = const MethodChannel(_pluginChannelPrefix);

class WebviewValue {
  WebviewValue({
    required this.isInitialized,
  });

  final bool isInitialized;

  WebviewValue copyWith({
    bool? isInitialized,
  }) {
    return WebviewValue(
      isInitialized: isInitialized ?? this.isInitialized,
    );
  }

  WebviewValue.uninitialized()
      : this(
          isInitialized: false,
        );
}

/// Controls a WebView and provides streams for various change events.
class WebviewController extends ValueNotifier<WebviewValue> {
  late Completer<void> _creatingCompleter;
  int _textureId = 0;
  bool _isDisposed = false;

  late MethodChannel _methodChannel;
  late EventChannel _eventChannel;

  final StreamController<String> _urlStreamController =
      StreamController<String>();

  /// A stream reflecting the current URL.
  Stream<String> get url => _urlStreamController.stream;

  final StreamController<LoadingState> _loadingStateStreamController =
      StreamController<LoadingState>();

  /// A stream reflecting the current loading state.
  Stream<LoadingState> get loadingState => _loadingStateStreamController.stream;

  final StreamController<String> _titleStreamController =
      StreamController<String>();

  /// A stream reflecting the current document title.
  Stream<String> get title => _titleStreamController.stream;

  final StreamController<SystemMouseCursor> _cursorStreamController =
      StreamController<SystemMouseCursor>();

  /// A stream reflecting the current cursor style.
  Stream<SystemMouseCursor> get _cursor => _cursorStreamController.stream;

  final StreamController<Map<dynamic, String>> _webMessageStreamController =
      StreamController<Map<dynamic, String>>();

  Stream<Map<dynamic, String>> get webMessage =>
      _webMessageStreamController.stream;

  WebviewController() : super(WebviewValue.uninitialized());

  /// Initializes the underlying platform view.
  Future<void> initialize() async {
    if (_isDisposed) {
      return Future<void>.value();
    }
    try {
      _creatingCompleter = Completer<void>();

      final reply =
          await _pluginChannel.invokeMapMethod<String, dynamic>('initialize');

      if (reply != null) {
        _textureId = reply['textureId'];
        value = value.copyWith(isInitialized: true);
        _methodChannel = MethodChannel('$_pluginChannelPrefix/$_textureId');
        _eventChannel =
            EventChannel('$_pluginChannelPrefix/$_textureId/events');
        _eventChannel.receiveBroadcastStream().listen((event) {
          final map = event as Map<dynamic, dynamic>;
          switch (map['type']) {
            case 'urlChanged':
              _urlStreamController.add(map['value']);
              break;
            case 'loadingStateChanged':
              final value = LoadingState.values[map['value']];
              _loadingStateStreamController.add(value);
              break;
            case 'titleChanged':
              _titleStreamController.add(map['value']);
              break;
            case 'cursorChanged':
              _cursorStreamController.add(_getCursorByName(map['value']));
              break;
            case 'webMessageReceived':
              try {
                final message = json.decode(map['value']);
                _webMessageStreamController.add(message);
              } catch (ex) {
                _webMessageStreamController.addError(ex);
              }
          }
        });
      }
    } on PlatformException {
      // TODO: handle PlatformException
    }

    _creatingCompleter.complete();
    return _creatingCompleter.future;
  }

  @override
  Future<void> dispose() async {
    await _creatingCompleter.future;
    if (!_isDisposed) {
      await _methodChannel.invokeMethod('dispose', _textureId);
      _isDisposed = true;
    }
    _isDisposed = true;
    super.dispose();
  }

  /// Loads the given [url].
  Future<void> loadUrl(String url) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('loadUrl', url);
  }

  /// Loads a document from the given string.
  Future<void> loadStringContent(String content) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('loadStringContent', content);
  }

  /// Reloads the current document.
  Future<void> reload() async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('reload');
  }

  /// Executes the given [script].
  Future<void> executeScript(String script) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('executeScript', script);
  }

  /// Posts the given JSON-formatted message to the current document.
  Future<void> postWebMessage(String message) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('postWebMessage', message);
  }

  /// Sets the user agent value.
  Future<void> setUserAgent(String value) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('setUserAgent', value);
  }

  /// Moves the virtual cursor to [position].
  Future<void> _setCursorPos(Offset position) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel
        .invokeMethod('setCursorPos', [position.dx, position.dy]);
  }

  /// Indicates whether the specified [button] is currently down.
  Future<void> _setPointerButtonState(PointerButton button, bool isDown) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('setPointerButton',
        <String, dynamic>{'button': button.index, 'isDown': isDown});
  }

  /// Sets the horizontal and vertical scroll delta.
  Future<void> _setScrollDelta(double dx, double dy) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('setScrollDelta', [dx, dy]);
  }

  /// Sets the surface size to the provided [size].
  Future<void> _setSize(Size size) async {
    if (_isDisposed) {
      return;
    }
    await _methodChannel.invokeMethod('setSize', [size.width, size.height]);
  }
}

class Webview extends StatefulWidget {
  const Webview(this.controller);

  final WebviewController controller;

  @override
  _WebviewState createState() => _WebviewState();
}

class _WebviewState extends State<Webview> {
  final GlobalKey _key = GlobalKey();
  final _downButtons = <int, PointerButton>{};

  MouseCursor _cursor = SystemMouseCursors.basic;

  WebviewController get _controller => widget.controller;

  @override
  void initState() {
    super.initState();

    // Report initial surface size
    WidgetsBinding.instance!.addPostFrameCallback((_) => _reportSurfaceSize());

    _controller._cursor.listen((cursor) {
      setState(() {
        _cursor = cursor;
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return _controller.value.isInitialized
        ? Container(
            color: Colors.transparent,
            child: NotificationListener<SizeChangedLayoutNotification>(
              onNotification: (notification) {
                _reportSurfaceSize();
                return true;
              },
              child: SizeChangedLayoutNotifier(
                  key: const Key('size-notifier'),
                  child: Container(
                    key: _key,
                    child: Listener(
                      onPointerHover: (ev) {
                        _controller._setCursorPos(ev.localPosition);
                      },
                      onPointerDown: (ev) {
                        final button = _getButton(ev.buttons);
                        _downButtons[ev.pointer] = button;
                        _controller._setPointerButtonState(button, true);
                      },
                      onPointerUp: (ev) {
                        final button = _downButtons.remove(ev.pointer);
                        if (button != null) {
                          _controller._setPointerButtonState(button, false);
                        }
                      },
                      onPointerCancel: (ev) {
                        final button = _downButtons.remove(ev.pointer);
                        if (button != null) {
                          _controller._setPointerButtonState(button, false);
                        }
                      },
                      onPointerMove: (move) {
                        _controller._setCursorPos(move.localPosition);
                      },
                      onPointerSignal: (signal) {
                        if (signal is PointerScrollEvent) {
                          _controller._setScrollDelta(
                              -signal.scrollDelta.dx, -signal.scrollDelta.dy);
                        }
                      },
                      child: MouseRegion(
                          cursor: _cursor,
                          child: Texture(textureId: _controller._textureId)),
                    ),
                  )),
            ),
          )
        : Container();
  }

  void _reportSurfaceSize() {
    final box = _key.currentContext?.findRenderObject() as RenderBox?;
    if (box != null) {
      _controller._setSize(box.size);
    }
  }
}
