import 'package:flutter/rendering.dart';
import 'package:flutter/widgets.dart';

class RenderWebview extends LeafRenderObjectWidget {
  const RenderWebview({
    Key? key,
    required this.textureId,
    required this.filterQuality,
    required this.onSizeChanged,
  }) : super(key: key);

  final int textureId;
  final FilterQuality filterQuality;
  final void Function(Size) onSizeChanged;

  @override
  RenderObject createRenderObject(BuildContext context) {
    return WebviewBox(
        textureId: textureId,
        filterQuality: filterQuality,
        onSizeChanged: onSizeChanged);
  }

  @override
  void updateRenderObject(BuildContext context, WebviewBox renderObject) {
    renderObject.textureId = textureId;
    renderObject.filterQuality = filterQuality;
    renderObject.onSizeChanged = onSizeChanged;
  }
}

class WebviewBox extends RenderBox {
  WebviewBox({
    required int textureId,
    FilterQuality filterQuality = FilterQuality.low,
    required this.onSizeChanged,
  })  : _textureId = textureId,
        _filterQuality = filterQuality;

  void Function(Size) onSizeChanged;

  Size? _lastNotifiedSize;

  int get textureId => _textureId;
  int _textureId;
  set textureId(int value) {
    if (value != _textureId) {
      _textureId = value;
      markNeedsPaint();
    }
  }

  FilterQuality get filterQuality => _filterQuality;
  FilterQuality _filterQuality;
  set filterQuality(FilterQuality value) {
    if (value != _filterQuality) {
      _filterQuality = value;
      markNeedsPaint();
    }
  }

  @override
  bool get sizedByParent => true;

  @override
  bool get alwaysNeedsCompositing => true;

  @override
  bool get isRepaintBoundary => true;

  @override
  @protected
  Size computeDryLayout(covariant BoxConstraints constraints) {
    return constraints.biggest;
  }

  @override
  void performLayout() {
    if (_lastNotifiedSize != size) {
      _lastNotifiedSize = size;
      onSizeChanged(size);
    }
  }

  @override
  bool hitTestSelf(Offset position) => true;

  @override
  void paint(PaintingContext context, Offset offset) {
    context.addLayer(
      TextureLayer(
        rect: Rect.fromLTWH(offset.dx, offset.dy, size.width, size.height),
        textureId: _textureId,
        filterQuality: _filterQuality,
      ),
    );
  }
}
