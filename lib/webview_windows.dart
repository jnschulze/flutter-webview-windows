import 'dart:async';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

// Order must match WebviewLoadingState (see webview.h)
enum LoadingState { None, Loading, NavigationCompleted }

// Order must match WebviewPointerButton (see webview.h)
enum PointerButton { None, Primary, Secondary, Tertiary }

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

class WebviewController extends ValueNotifier<WebviewValue> {
  late Completer<void> _creatingCompleter;
  int _textureId = 0;
  bool _isDisposed = false;

  late MethodChannel _methodChannel;
  late EventChannel _eventChannel;

  final StreamController<String> _urlStreamController =
      StreamController<String>();
  Stream<String> get url => _urlStreamController.stream;

  final StreamController<LoadingState> _loadingStateStreamController =
      StreamController<LoadingState>();
  Stream<LoadingState> get loadingState => _loadingStateStreamController.stream;

  WebviewController() : super(WebviewValue.uninitialized());

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
              _urlStreamController.add(map['url']);
              break;
            case 'loadingStateChanged':
              final value = LoadingState.values[map['value']];
              _loadingStateStreamController.add(value);
              break;
          }
        });
      }
    } on PlatformException catch (e) {}

    _creatingCompleter.complete();
    return _creatingCompleter.future;
  }

  void loadUrl(String url) {
    _methodChannel.invokeMethod('loadUrl', url);
  }

  void setCursorPos(Offset pos) {
    _methodChannel.invokeMethod('setCursorPos', [pos.dx, pos.dy]);
  }

  void setPointerButtonState(PointerButton button, bool isDown) {
    _methodChannel.invokeMethod('setPointerButton',
        <String, dynamic>{'button': button.index, 'isDown': isDown});
  }

  void setScrollDelta(double dx, double dy) {
    _methodChannel.invokeMethod('setScrollDelta', [dx, dy]);
  }

  void setSize(Size size) {
    _methodChannel.invokeMethod('setSize', [size.width, size.height]);
  }
}

class Webview extends StatefulWidget {
  const Webview(this.controller);

  final WebviewController controller;

  @override
  _WebviewState createState() => _WebviewState();
}

class _WebviewState extends State<Webview> {
  GlobalKey _key = GlobalKey();

  @override
  void initState() {
    super.initState();
  }

  final Map<int, PointerButton> _downButtons = Map<int, PointerButton>();

  @override
  Widget build(BuildContext context) {
    return widget.controller.value.isInitialized
        ? Container(
            color: Colors.pink,
            child: NotificationListener<SizeChangedLayoutNotification>(
              onNotification: (notification) {
                reportSurfaceSize();
                return true;
              },
              child: SizeChangedLayoutNotifier(
                  key: const Key('size-notifier'),
                  child: Container(
                    key: _key,
                    child: Listener(
                      onPointerHover: (ev) {
                        widget.controller.setCursorPos(ev.localPosition);
                      },
                      onPointerDown: (ev) {
                        final button = _getButton(ev.buttons);
                        _downButtons[ev.pointer] = button;
                        widget.controller.setPointerButtonState(button, true);
                      },
                      onPointerUp: (ev) {
                        final button = _downButtons.remove(ev.pointer);
                        if (button != null) {
                          widget.controller
                              .setPointerButtonState(button, false);
                        }
                      },
                      onPointerCancel: (ev) {
                        final button = _downButtons.remove(ev.pointer);
                        if (button != null) {
                          widget.controller
                              .setPointerButtonState(button, false);
                        }
                      },
                      onPointerMove: (move) {
                        widget.controller.setCursorPos(move.localPosition);
                      },
                      onPointerSignal: (signal) {
                        if (signal is PointerScrollEvent) {
                          widget.controller.setScrollDelta(
                              -signal.scrollDelta.dx, -signal.scrollDelta.dy);
                        }
                      },
                      child: Texture(textureId: widget.controller._textureId),
                    ),
                  )),
            ),
          )
        : Container();
  }

  void reportSurfaceSize() {
    final box = _key.currentContext?.findRenderObject() as RenderBox?;
    if (box != null) {
      widget.controller.setSize(box.size);
    }
  }
}
