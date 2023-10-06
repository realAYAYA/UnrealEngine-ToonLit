// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';

/// A widget that reports the size of its child as soon as it's laid out.
class Measurable extends SingleChildRenderObjectWidget {
  const Measurable({Key? key, required super.child, required this.onMeasured}) : super(key: key);

  /// Callback function which will be called whenever the [child] is laid out and reports its new [size].
  final void Function(Size size) onMeasured;

  @override
  RenderObject createRenderObject(BuildContext context) => _MeasurableRenderObject(onMeasured);
}

class _MeasurableRenderObject extends RenderProxyBox {
  _MeasurableRenderObject(this.onMeasured);

  /// Callback function which will be called whenever the [child] is laid out and reports its new [size].
  final void Function(Size size) onMeasured;

  /// The previous size of this object.
  Size? _lastSize;

  @override
  void performLayout() {
    super.performLayout();

    final Size? size = child?.size;
    if (size == _lastSize) {
      return;
    }

    _lastSize = size;
    if (size == null) {
      return;
    }

    // Report the new size post-frame to avoid triggering a rebuild in the middle of another rebuild.
    WidgetsBinding.instance.addPostFrameCallback((_) => onMeasured(size));
  }
}
